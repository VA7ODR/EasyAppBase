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


#include "easyappbase.hpp"

#if !defined APP_NAME
#define APP_NAME "SampleApp"
#endif
#if !defined APP_VERSION_STRING
#define APP_VERSION_STRING "0.0.0"
#endif

// Sample Window
class SampleWindow : public EasyAppBase
{
	public:
		SampleWindow() : EasyAppBase("sample", "Sample Window") {}

		virtual void Start() override; // Optional.  This is where you would start threads, load things, etc. If it takes a long time, use a thread with THREAD instead of std::thread.
		virtual void Render(bool * bShow) override;  // REQUIRED.  This is where you would render your window.
		virtual void Stop() override; // This is where you would clean up, like joining threads.
		virtual bool BuildsOwnWindow() override { return false; }  // Set this to true, if you want to have tighter control over the ImGui::Begin and ImGui::End calls. If set to False, EasyAppBase will handle it for you.

	private:
		Thread sampleThread;
		SharedRecursiveMutex m_mutex;
		int iCount = 0;
		int iButtonCount = 0;
		EventHandler::Event buttonEvent = CreateEvent("ButtonEvent", EventHandler::auto_reset);
		EventHandler::Event stopEvent = CreateEvent("ButtonEvent", EventHandler::manual_reset);
};

void SampleWindow::Start()
{
	sampleThread = THREAD("SampleThread", [&](std::stop_token stoken)
	{
		bool bRun = true;
		while (!stoken.stop_requested() && bRun) {
			int iEventIndex = EventHandlerWait({buttonEvent, stopEvent}, 1000ms);
			switch (iEventIndex) {
				case 0:
				{
					RecursiveSharedLock lock(m_mutex);
					iButtonCount++;
					Log(AppLogger::INFO) << "Button Count: " << iButtonCount;
					break;
				}

				case 1:
				{
				   bRun = false;
				   Log(AppLogger::INFO) << "Exit Event.";
				   break;
				}

			   case EventHandler::TIMEOUT:
			   {
				   RecursiveExclusiveLock lock(m_mutex);
				   iCount++;
				   Log(AppLogger::INFO) << "Timeout Count: " << iCount;
				   break;
			   }

			   case EventHandler::EXIT_ALL:
			   {
				   bRun = false;
				   Log(AppLogger::INFO) << "EventHandler::ExitAll Event.";
				   break;
			   }
			}
		}
		Log(AppLogger::INFO) << "SampleThread Exited.";
	});
}

void SampleWindow::Stop()
{
	EventHandler::Set(stopEvent);
}

// Sample Window Render
void SampleWindow::Render(bool * bShow)
{
	ImGui::Text("Sample Window");
	ImGui::Text("Button Count: %d", iButtonCount);
	ImGui::Text("Timeout Count: %d", iCount);
	if (ImGui::Button("Button")) {
		EventHandler::Set(buttonEvent);
	}
}

void MainRenderer()
{
	ImGui::Text("Hello, World!");
}

// Main code
int main(int, char**)
{
	// Register the Sample Window
	// EasyAppBase::GenerateWindow<SampleWindow>();

	// Set the Main Renderer
	EasyAppBase::SetMainRenderer([&]() { MainRenderer(); });

	// Uncomment the following lines to enable/disable features

	// EasyAppBase::DisableDemo(true); // Uncomment this to disable the ImGui Demo Window

	// EasyAppBase::DisableDocking(true); // Uncomment this to disable docking (see https://github.com/ocornut/imgui/issues/2109 for details)

	// EasyAppBase::DisableViewports(true); // Uncomment this to disable viewports (see https://github.com/ocornut/imgui/issues/1542 for details)

	// EasyAppBase::DisableGUI(true); // Uncomment this and the following line to disable the GUI entirely
	// Thread killEasyAppThread = THREAD("Kill Thread", [](std::stop_token /*stoken*/) { std::this_thread::sleep_for(5s); EasyAppBase::ExitAll(); }); // Uncomment this example to exit the application after 5 seconds when no gui is present.

	// EasyAppBase::SetNetworkThreads(4); // Uncomment this to set the number of network threads to 4. 0 or less is no network. Default is 0.

	// Run the application
	return EasyAppBase::Run(APP_NAME, APP_NAME " v" APP_VERSION_STRING);
}
