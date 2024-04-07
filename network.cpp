/*
Copyright (c) 2012-2024 James Baker

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

The official repository for this library is at https://github.com/VA7ODR/EasyAppBase

*/

#include "network.hpp"
#include "app_logger.hpp"
#include "thread.hpp"

#include <boost/asio/ssl.hpp>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

namespace Network
{
	#ifdef _WIN32
	#include <windows.h>
	#include <wincrypt.h>

	std::string getWindowsCertificates() {
		HCERTSTORE hStore = CertOpenSystemStore(0, "ROOT");
		if (!hStore) {
			std::cerr << "Failed to open certificate store\n";
			return "";
		}

		PCCERT_CONTEXT pContext = nullptr;
		std::string certs;
		while ((pContext = CertEnumCertificatesInStore(hStore, pContext)) != nullptr) {
			DWORD size = 0;
			BOOL result = CryptStringToBinaryA(
				reinterpret_cast<const char*>(pContext->pbCertEncoded),
				pContext->cbCertEncoded,
				CRYPT_STRING_BASE64,
				NULL,
				&size,
				NULL,
				NULL);

			if (!result) {
				std::cerr << "Error converting certificate to BASE64\n";
				continue;
			}

			std::vector<BYTE> buffer(size);
			result = CryptStringToBinaryA(
				reinterpret_cast<const char*>(pContext->pbCertEncoded),
				pContext->cbCertEncoded,
				CRYPT_STRING_BASE64,
				buffer.data(),
				&size,
				NULL,
				NULL);

			if (result) {
				certs += "-----BEGIN CERTIFICATE-----\n";
				certs.append(reinterpret_cast<char*>(buffer.data()), size);
				certs += "-----END CERTIFICATE-----\n";
			}
		}

		CertCloseStore(hStore, 0);

		return certs;
	}
#else

	std::string readCertificateFile(const std::string& path) {
		std::ifstream file(path);
		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}

	std::string getDefaultCertificates() {
		#ifdef __APPLE__
		std::string path = "/etc/ssl/cert.pem"; // Common path on macOS
		#else
		std::string path = "/etc/ssl/certs/ca-certificates.crt"; // Common path on Linux
		#endif
		return readCertificateFile(path);
	}
#endif

	std::string getCertificates() {
		#ifdef _WIN32
		return getWindowsCertificates();
		#else
		return getDefaultCertificates();
		#endif
	}

	std::string URLEncode(const std::string &in)
	{
		std::string out;

		for (size_t i = 0; i < in.size(); ++i) {
			auto & c = in[i];
			if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
				out += c;
			} else if (c == '%') {
				if (i + 2 < in.size()) {
					if (std::isxdigit(in[i + 1]) && std::isxdigit(in[i + 2])) {
						out += in.substr(i, 3);
						i += 2;
						continue;
					}
				}
				out += "%25";
			} else if (c == ' ') {
				out += '+';
			} else {
				out += '%';
				out += "0123456789ABCDEF"[((unsigned char)c) >> 4];
				out += "0123456789ABCDEF"[((unsigned char)c) & 15];
			}
		}

		return out;
	}

	std::string URLDecode(const std::string &in)
	{
		std::string out;

		for (size_t i = 0; i < in.size(); ++i) {
			if (in[i] == '%') {
				if (i + 2 < in.size()) {
					auto c = std::stoi(in.substr(i + 1, 2), 0, 16);
					out += c;
					i += 2;
				}
			} else if (in[i] == '+') {
				out += ' ';
			} else {
				out += in[i];
			}
		}

		return out;
	}

	CoreBase::CoreBase(int threadCountIn) :
		ioc(threadCountIn),
		sCertificates(getCertificates())
	{
		vThreads.reserve(threadCountIn - 1);
		for(auto i = threadCountIn - 1; i >= 0; --i) {
			vThreads.push_back(THREAD("Network::core::" + std::to_string(i), [&](int iThreadNumber)
			{
				while (!bExit) {
					auto iEvent = EventHandlerWait({eWakeUp, eExit}, EventHandler::INFINITE);
					switch (iEvent) {
						case 0:
							if (ioc.stopped()) {
								ioc.restart();
							}
							ioc.run();
							break;

						default:
							bExit = true;
							break;
					}
				}
			}, i));
		}
	}

	CoreBase::~CoreBase()
	{
		Exit();
	}

	void CoreBase::Exit()
	{
		EventHandler::Set(eExit);
		bExit = true;
		for(auto &thread : vThreads) {
			thread->get_thread().request_stop();
			thread->get_thread().join();
		}
		vThreads.clear();
	}

	void CoreBase::WakeUp()
	{
		EventHandler::Set(eWakeUp);
	}

	boost::asio::io_context &CoreBase::IOContext()
	{
		return ioc;
	}

	const std::string &CoreBase::Certificates() const
	{
		return sCertificates;
	}

	core_t & Core(int iThreadCountInit)
	{
		static std::mutex initMutex;
		std::lock_guard<std::mutex> lock(initMutex);
		static core_t core = std::make_shared<CoreBase>(iThreadCountInit);
		return core;
	}

	core_t & Init(int iThreadCountInit)
	{
		return Core(iThreadCountInit);
	}

	namespace HTTP
	{
		ClientBase::ClientBase(const std::string &sAddressIn, int iPortIn, bool bSSLIn, bool bAllowSelfSignedIn) :
			core(Core()),
			sAddress(sAddressIn),
			iPort(iPortIn),
			resolver(core->IOContext()),
			bSSL(bSSLIn),
			bAllowSelfSigned(bAllowSelfSignedIn)
		{
			Log(AppLogger::DEBUG) << "ClientTCP::ClientTCP " << sAddress << ":" << iPort << std::endl;
		}

		ClientBase::~ClientBase()
		{
			if (stream && tcp_stream().socket().is_open()) {
				boost::system::error_code ec;
				tcp_stream().socket().close(ec);
			}
			Log(AppLogger::DEBUG) << "ClientTCP::~ClientTCP " << sAddress << ":" << iPort << std::endl;
		}

		void ClientBase::KeepAlive(bool bKeepAliveIn)
		{
			bKeepAlive = bKeepAliveIn;
		}

		bool ClientBase::KeepAlive()
		{
			return bKeepAlive;
		}

		bool ClientBase::Connected()
		{
			if (!stream) {
				PrepStream();
			}
			return tcp_stream().socket().is_open();
		}

		void ClientBase::Close()
		{
			if (stream) {
				Log(AppLogger::DEBUG) << "ClientTCP::Close " << sAddress << ":" << iPort << std::endl;
				boost::system::error_code ec;
				tcp_stream().socket().close(ec);
			}
		}

		void ClientBase::Request(request_t reqIn, handler_t handlerIn, std::chrono::seconds timeout, bool bKeepAliveIn)
		{
			Log(AppLogger::DEBUG) << "ClientTCP::Request " << sAddress << ":" << iPort << std::endl;
			req = reqIn;
			res = std::make_shared<http::response<http::string_body>>();
			req->target() = URLEncode(req->target());
			if (req->target().empty()) {
				req->target("/");
			}

			handler = handlerIn;
			bKeepAlive = bKeepAliveIn;
			req->keep_alive(bKeepAlive);
			bool bConnected = Connected();
			tcp_stream().expires_after(timeout);
			if (bConnected) {
				do_write();
			} else {
				do_resolve();
			}
			core->WakeUp();
		}

		void ClientBase::Head(const std::string &sPath, handler_t handlerIn, std::chrono::seconds timeout, bool bKeepAliveIn)
		{
			Log(AppLogger::DEBUG) << "ClientTCP::Head " << sAddress << ":" << iPort << std::endl;
			auto req = std::make_shared<http::request<http::string_body>>(http::verb::head, sPath, 11);
			req->set(http::field::host, sAddress/* + ":" + std::to_string(iPort)*/);
			req->set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
			Request(req, handlerIn, timeout, bKeepAliveIn);
		}

		void ClientBase::Get(const std::string &sPath, handler_t handlerIn, std::chrono::seconds timeout, bool bKeepAliveIn)
		{
			Log(AppLogger::DEBUG) << "ClientTCP::Get " << sAddress << ":" << iPort << std::endl;
			auto req = std::make_shared<http::request<http::string_body>>(http::verb::get, sPath, 11);
			req->set(http::field::host, sAddress/* + ":" + std::to_string(iPort)*/);
			req->set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
			Request(req, handlerIn, timeout, bKeepAliveIn);
		}

		void ClientBase::Put(const std::string &sPath, const std::string &sBody, const std::string &sContentType, handler_t handlerIn, std::chrono::seconds timeout, bool bKeepAliveIn)
		{
			Log(AppLogger::DEBUG) << "ClientTCP::Put " << sAddress << ":" << iPort << std::endl;
			auto req = std::make_shared<http::request<http::string_body>>(http::verb::put, sPath, 11);
			req->set(http::field::host, sAddress/* + ":" + std::to_string(iPort)*/);
			req->set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
			req->set(http::field::content_type, sContentType);
			req->body() = sBody;
			req->prepare_payload();
			Request(req, handlerIn, timeout, bKeepAliveIn);
		}

		void ClientBase::Post(const std::string &sPath, const std::string &sBody, const std::string &sContentType, handler_t handlerIn, std::chrono::seconds timeout, bool bKeepAliveIn)
		{
			Log(AppLogger::DEBUG) << "ClientTCP::Post " << sAddress << ":" << iPort << std::endl;
			auto req = std::make_shared<http::request<http::string_body>>(http::verb::post, sPath, 11);
			req->set(http::field::host, sAddress/* + ":" + std::to_string(iPort)*/);
			req->set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
			req->set(http::field::content_type, sContentType);
			req->body() = sBody;
			req->prepare_payload();
			Request(req, handlerIn, timeout, bKeepAliveIn);
		}

		void ClientBase::Delete(const std::string &sPath, handler_t handlerIn, std::chrono::seconds timeout, bool bKeepAliveIn)
		{
			Log(AppLogger::DEBUG) << "ClientTCP::Delete " << sAddress << ":" << iPort << std::endl;
			auto req = std::make_shared<http::request<http::string_body>>(http::verb::delete_, sPath, 11);
			req->set(http::field::host, sAddress/* + ":" + std::to_string(iPort)*/);
			req->set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
			Request(req, handlerIn, timeout, bKeepAliveIn);
		}

		beast::tcp_stream &ClientBase::tcp_stream() {
			return stream->next_layer();
		}

		beast::ssl_stream<beast::tcp_stream> &ClientBase::ssl_stream() {
			return *stream;
		}

		void ClientBase::PrepStream()
		{
			if (!stream) {
				Log(AppLogger::DEBUG) << "ClientTCP::PrepStream " << sAddress << ":" << iPort << std::endl;
				stream = std::make_shared<beast::ssl_stream<beast::tcp_stream>>(core->IOContext(), ssl_ctx);
			}
		}

		void ClientBase::do_resolve()
		{
			Log(AppLogger::DEBUG) << "ClientTCP::do_resolve " << sAddress << ":" << iPort << std::endl;
			auto self = shared_from_this();
			resolver.async_resolve(sAddress, std::to_string(iPort), beast::bind_front_handler([&, self] (boost::system::error_code ec, tcp::resolver::results_type results)
			{
				if (ec) {
					Log(AppLogger::ERROR) << "ClientBase::on_resolve Error: " << ec.message() << ": " << sAddress << ":" << iPort << std::endl;
					return;
				}

				resolve_results = results;
				do_connect();
			}));
			core->WakeUp();
		}

		void ClientBase::do_connect()
		{
			Log(AppLogger::DEBUG) << "ClientTCP::do_connect " << sAddress << ":" << iPort << std::endl;
			auto self = shared_from_this();
			tcp_stream().async_connect(resolve_results, beast::bind_front_handler([&, self] (boost::system::error_code ec, tcp::resolver::results_type::endpoint_type)
			{
				if (ec) {
					Log(AppLogger::ERROR) << "ClientBase::on_connect Error: " << ec.message() << ": " << sAddress << ":" << iPort << std::endl;
					return;
				}
				if (bSSL) {
					do_handshake();
				} else {
					do_write();
				}
			}));
			core->WakeUp();
		}

		void ClientBase::do_handshake()
		{
			Log(AppLogger::DEBUG) << "ClientTCP::do_handshake " << sAddress << ":" << iPort << std::endl;
			auto self = shared_from_this();
			ssl_stream().async_handshake(ssl::stream_base::client, beast::bind_front_handler([&, self] (boost::system::error_code ec)
			{
				if (ec) {
					Log(AppLogger::ERROR) << "ClientBase::on_handshake Error: " << ec.message() << ": " << sAddress << ":" << iPort << std::endl;
					return;
				}
				do_write();
			}));
			core->WakeUp();
		}

		void ClientBase::do_write()
		{
			Log(AppLogger::DEBUG) << "ClientTCP::do_write " << sAddress << ":" << iPort << std::endl;
			auto self = shared_from_this();
			auto write_handler = [&, self] (boost::system::error_code ec, std::size_t bytes_transferred)
			{
				boost::ignore_unused(bytes_transferred);
				if (ec) {
					Log(AppLogger::ERROR) << "ClientBase::on_write Error: " << ec.message() << ": " << sAddress << ":" << iPort << std::endl;
					return;
				}
				do_read();
			};
			if (bSSL) {
				http::async_write(ssl_stream(), *req, beast::bind_front_handler(write_handler));
			} else {
				http::async_write(tcp_stream(), *req, beast::bind_front_handler(write_handler));
			}

			core->WakeUp();
		}

		void ClientBase::do_read()
		{
			Log(AppLogger::DEBUG) << "ClientTCP::do_read " << sAddress << ":" << iPort << std::endl;
			auto self = shared_from_this();
			auto read_handler = [&, self] (boost::system::error_code ec, std::size_t bytes_transferred)
			{
				if (ec) {
					Log(AppLogger::ERROR) << "ClientBase::on_read Error: " << ec.message() << ": " << sAddress << ":" << iPort << std::endl << *res << std::endl;
					return;
				}
				handler(req, res, sAddress, iPort);
			};
			if (bSSL) {
				http::async_read(ssl_stream(), buffer, *res, beast::bind_front_handler(read_handler));
			} else {
				http::async_read(tcp_stream(), buffer, *res, beast::bind_front_handler(read_handler));
			}
			core->WakeUp();
		}

		client_t Client(const std::string &sAddress, int iPort, bool bSSLIn, bool bAllowSelfSignedIn)
		{
			return std::make_shared<ClientBase>(sAddress, iPort, bSSLIn, bAllowSelfSignedIn);
		}
	} // namespace HTTP
} // namespace Network
