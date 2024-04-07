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

#pragma once
#include <chrono>
#include <memory>
#include <vector>

namespace EventHandler
{
	enum event_type
	{
		manual_reset,
		auto_reset
	};

	class EventBase
	{
		public:
			EventBase(const std::string & sNameIn, event_type eTypeIn) : sName(sNameIn), eType(eTypeIn) {}

			void Set();
			void Reset();

			const std::string & Name();

		protected:
			friend class CleanupAfterWait;
			friend class Map;
			friend int Wait(const std::string & sFile, const std::string & sFunc, int iLine, std::vector<std::shared_ptr<EventBase>>, std::chrono::milliseconds);
			void Waiting();
			void AutoReset();
			std::string sName;
			size_t waitCount = 0;
			event_type eType = manual_reset;
			bool bValue = false;
	};

	using Event = std::shared_ptr<EventBase>;

	const std::chrono::milliseconds INFINITE = std::chrono::milliseconds::max();

	int Wait(const std::string & sFile, const std::string & sFunc, int iLine, std::vector<Event> vEvents, std::chrono::milliseconds timeout = std::chrono::milliseconds::max());

	void Set(Event e);

	void Reset(Event e);

	Event CreateEvent(const std::string & sName, event_type eType = manual_reset);

	void ExitAll();

	const int TIMEOUT = -1;
	const int EXIT_ALL = -2;

} // EventHandler

#define EventHandlerWait(...) EventHandler::Wait(SOURCE_FILE, __func__, __LINE__, __VA_ARGS__)
