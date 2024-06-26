/*
Copyright (c) 2024 James Baker

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

#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/serial_port.hpp>

#include <memory>
#include <vector>

#include "eventhandler.hpp"
#include "thread.hpp"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

using namespace std::chrono_literals;

namespace Network
{
	template <typename T>
	T swapEndianness(T value)
	{
		static_assert(std::is_arithmetic<T>::value, "Only arithmatic types are supported");
		union {
			T value;
			uint8_t bytes[sizeof(T)];
		} src, dest;

		src.value = value;

		for (size_t i = 0; i < sizeof(T); ++i) {
			dest.bytes[i] = src.bytes[sizeof(T) - i - 1];
		}

		return dest.value;
	}

	std::string URLEncode(const std::string & in);
	std::string URLDecode(const std::string & in);

	class CoreBase
	{
		public:
			CoreBase(int threadCountIn);
			~CoreBase();

			void                Exit();
			void                WakeUp() const;
			net::io_context &   IOContext();
			const std::string & Certificates() const;

		protected:
			net::io_context ioc;
			EventHandler::Event eWakeUp = EventHandler::CreateEvent("HTTP::Core::WakeUp", EventHandler::auto_reset);
			EventHandler::Event eExit = EventHandler::CreateEvent("HTTP::Core::Exit", EventHandler::manual_reset);
			std::vector<Thread> vThreads;
			std::string sCertificates;
			bool bExit = false;
	};

	using core_t = std::shared_ptr<CoreBase>;
	core_t & Core(int iThreadCountInit = 0);  // calling this with <= 0 will not create an instance if one does not exist. Only the first call to this > 0 will create the instance.
	void ExitAll();

	class Serial
	{
		public:
			// Serial port class using Boost::asio:

			Serial(const std::string & sPortIn, int iBaudRateIn, int iDataBitsIn = 8, int iStopBitsIn = 1, int iParityIn = 0, int iFlowControlIn = 0, int iTimeoutIn = 1000);
			~Serial();

			bool IsOpen() const;
			bool Open();
			bool Close();

			void Write(const std::string & sData);
			void SetReadCalback(std::function<void(const std::string & sData)> callback);

			static std::deque<std::string> ListPorts();

		private:
			void DoRead();

			core_t core;
			std::string sPort;
			int iBaudRate;
			int iDataBits;
			int iStopBits;
			int iParity;
			int iFlowControl;
			int iTimeout;
			boost::asio::serial_port port;
			std::function<void(const std::string & sData)> readCallback;
			std::string sReadData;
			mutable std::recursive_mutex mtx;
	};

	namespace HTTP
	{
		using verb = http::verb;
		using request_t = std::shared_ptr<http::request<http::string_body>>;
		using response_t = std::shared_ptr<http::response<http::string_body>>;
		using handler_t = std::function<void(request_t req, response_t res, const std::string & sRremoteAddr, int iRemotePort)>;

		class ClientBase : public std::enable_shared_from_this<ClientBase>
		{
			public:
				ClientBase(std::string  sAddressIn, int iPortIn, bool bSSLIn, bool bAllowSelfSignedIn = false);
				~ClientBase();

				void KeepAlive(bool bKeepAliveIn);
				bool KeepAlive() const;

				virtual bool Connected();
				virtual void Close();

				void Request(request_t reqIn, handler_t handlerIn, std::chrono::seconds timeout = 30s, bool bKeepAliveIn = false);

				void Head(const std::string & sPath, handler_t handlerIn, std::chrono::seconds timeout = 30s, bool bKeepAliveIn = false);
				void Get(const std::string & sPath, handler_t handlerIn, std::chrono::seconds timeout = 30s, bool bKeepAliveIn = false);
				void Put(const std::string & sPath, const std::string & sBody, const std::string & sContentType, handler_t handlerIn, std::chrono::seconds timeout = 30s, bool bKeepAliveIn = false);
				void Post(const std::string & sPath, const std::string & sBody, const std::string & sContentType, handler_t handlerIn, std::chrono::seconds timeout = 30s, bool bKeepAliveIn = false);
				void Delete(const std::string & sPath, handler_t handlerIn, std::chrono::seconds timeout = 30s, bool bKeepAliveIn = false);

			protected:
				beast::tcp_stream &                    tcp_stream() const;
				beast::ssl_stream<beast::tcp_stream> & ssl_stream() const;
				virtual void                           PrepStream();

				void do_resolve();
				virtual void do_connect();
				void do_handshake();
				void do_write();
				void do_read();

				core_t core;
				std::string sAddress;
				int iPort = 0;
				ssl::context ctx{ssl::context::tlsv12_client};
				tcp::resolver resolver;
				tcp::resolver::results_type resolve_results;
				handler_t handler;
				bool bKeepAlive = false;
				std::mutex mtx;
				request_t req;
				response_t res;
				beast::flat_buffer buffer;
				bool bThreadExited = false;

				bool bSSL= true;
				bool bAllowSelfSigned = false;
				std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> stream;
				boost::asio::ssl::context ssl_ctx{boost::asio::ssl::context::tlsv12_client};
		};

		using client_t = std::shared_ptr<ClientBase>;

		client_t Client(const std::string & sAddress, int iPort, bool bSSLIn = true, bool bAllowSelfSignedIn = false);

	} // HTTP
} // Network
#define HTTP_HANDLER_LAMBDA [&](Network::HTTP::request_t req, Network::HTTP::response_t res, const std::string & sRremoteAddr, int iRemotePort)
