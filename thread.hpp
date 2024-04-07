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

#include "app_logger.hpp"
#include <map>
#include <set>
#include <thread>

#if defined _WINDOWS

typedef HANDLE thread_id_t;
#define get_thread_id GetCurrentThreadId;

#else

#include <unistd.h>

typedef __pid_t thread_id_t;
#define get_thread_id gettid

#endif

class Thread
{
	public:
		Thread() {}
		Thread(const std::string & sFileIn, const std::string & sFunctionIn, int iLineIn, const std::string & sNameIn, auto __f)
		{
			std::lock_guard<std::mutex> lock(m_mutex());
			pSelf->sName = sNameIn;
			pSelf->sFunction = sFunctionIn;
			pSelf->iLine = iLineIn;
			pSelf->sFile = sFileIn;
			pSelf->parent_id = get_thread_id();
			start(__f);
		}

		Thread(Thread && other)
		{
			std::lock_guard<std::mutex> lock(m_mutex());
			pSelf = other.pSelf;
		}

		Thread & operator=(Thread && other)
		{
			std::lock_guard<std::mutex> lock(m_mutex());
			pSelf = other.pSelf;
			return *this;
		}

		~Thread()
		{
			std::lock_guard<std::mutex> lock(m_mutex());
			if (pSelf && pSelf.use_count() == 1 && pSelf->m_thread.joinable()) {
				pSelf->m_thread.request_stop();
				pSelf->m_thread.join();
			}
		}

		Thread& start(auto __f)
		{
			auto runner = [](std::stop_token stoken, std::shared_ptr<Data> pSelf, auto __f) {
				{
					std::lock_guard<std::mutex> lock(m_mutex());
					pSelf->id = get_thread_id();
					m_threads()[pSelf->id] = pSelf.get();
					if (m_threads().find(pSelf->parent_id) != m_threads().end()) {
						m_threads()[pSelf->parent_id]->children.insert(pSelf->id);
					} else {
						main_thread_children().insert(pSelf->id);
					}
				}
				__f(stoken);
				{
					std::lock_guard<std::mutex> lock(m_mutex());
					m_threads().erase(pSelf->id);
					if (m_threads().find(pSelf->parent_id) != m_threads().end()) {
						m_threads()[pSelf->parent_id]->children.erase(pSelf->id);
					} else {
						main_thread_children().erase(pSelf->id);
					}
					pSelf = nullptr;
				}
			};
			pSelf->m_thread = std::jthread(runner, pSelf, __f);
			return *this;
		}

		std::jthread & get_thread();

		void request_stop()
		{
			pSelf->m_thread.request_stop();
		}

		bool joinable()
		{
			return pSelf->m_thread.joinable();
		}

		void join()
		{
			if (pSelf->m_thread.joinable()) {

			}
			pSelf->m_thread.join();
		}

		static thread_id_t main_thread_id();

		struct Data {
			std::string sName;
			std::string sFile;
			std::string sFunction;
			int iLine = 0;
			thread_id_t id = 0;
			thread_id_t parent_id = 0;
			std::set<thread_id_t> children;
			std::jthread m_thread;
		};

		struct MapItem
		{
			std::string sFile;
			std::string sFunction;
			int iLine = 0;
			std::string sName;
			thread_id_t id = 0;
			thread_id_t parent_id = 0;
			std::set<MapItem> children;

			MapItem();

			MapItem(const Data & thread);

			bool operator<(const MapItem & other) const;
		};

		static MapItem map();
		static void log_map(AppLogger::LogLevel levelIn, const std::string &sFileIn, const std::string sFunctionIn, int iLineIn);

	private:
		std::shared_ptr<Data> pSelf = std::make_shared<Data>();

		static std::mutex & m_mutex();

		static std::set<thread_id_t> & main_thread_children();

		static std::map<thread_id_t, Data*> & m_threads();


};

#define THREAD(sName, func, ...) Thread(SOURCE_FILE, __FUNCTION__, __LINE__, sName, std::bind(func, std::placeholders::_1 __VA_OPT__(,) __VA_ARGS__))
#define LOG_THREAD_MAP(level) Thread::log_map(level, SOURCE_FILE, __FUNCTION__, __LINE__)

