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

The official repository for this library is at https://github.com/VA7ODR/json

*/

#include "thread.hpp"
#include <functional>

std::mutex &Thread::m_mutex()
{
	static std::mutex mMutex;
	return mMutex;
}

std::set<thread_id_t> &Thread::main_thread_children()
{
	static std::set<thread_id_t> vRet;
	return vRet;
}

std::map<thread_id_t, Thread *> &Thread::m_threads()
{
	static std::map<thread_id_t, Thread*> vRet;
	return vRet;
}

static thread_id_t & real_id()
{
	static thread_id_t id = 0;
	return id;
}

class GetMainThreadId
{
	public:
		GetMainThreadId()
		{
			real_id() = get_thread_id();
		}
};

GetMainThreadId get_main_thread_id;

std::jthread &Thread::get_thread()
{
	return m_thread;
}

thread_id_t Thread::main_thread_id()
{
	return real_id();
}

Thread::MapItem Thread::map()
{
	return MapItem();
}

void Thread::log_map(AppLogger::LogLevel levelIn, const std::string & sFileIn, const std::string sFunctionIn, int iLineIn)
{
	auto mMap = map();
	AppLogger logger(levelIn, sFileIn, sFunctionIn, iLineIn);
	logger << "Thread Map:\n";
	std::function<void(const Thread::MapItem &item, const std::string &indent, AppLogger &logger)> descend = [&](const Thread::MapItem &item, const std::string &indent, AppLogger &logger)
	{
		logger << indent << item.sName << " (" << item.id << "):\n";
		for (auto & child : item.children) {
			descend(child, indent + "    ", logger);
		}
	};
	descend(mMap, "", logger);
}

Thread::MapItem::MapItem()
{
	std::lock_guard<std::mutex> lock(m_mutex());
	sName = "Main Thread";
	id = main_thread_id();
	for (auto & child_id : main_thread_children()) {
		if (m_threads().find(child_id) != m_threads().end()) {
			children.insert(MapItem(*m_threads()[child_id]));
		}
	}
}

Thread::MapItem::MapItem(const Thread &thread)
{
	sName = thread.sName;
	sFile = thread.sFile;
	sFunction = thread.sFunction;
	iLine = thread.iLine;
	id = thread.id;
	parent_id = thread.parent_id;
	for (auto & child_id : thread.children) {
		if (m_threads().find(child_id) != m_threads().end()) {
			children.insert(MapItem(*m_threads()[child_id]));
		}
	}
}

bool Thread::MapItem::operator<(const MapItem &other) const
{
	return id < other.id;
}
