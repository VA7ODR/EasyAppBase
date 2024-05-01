#pragma once

#define EASY_APP_LICENSE \
	"Copyright (c) 2024 James Baker" \
	"\n" \
	"Permission is hereby granted, free of charge, to any person obtaining a copy " \
	"of this software and associated documentation files (the \"Software\"), to dea l" \
	"in the Software without restriction, including without limitation the rights " \
	"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell " \
	"copies of the Software, and to permit persons to whom the Software is " \
	"furnished to do so, subject to the following conditions:\n" \
	"\n" \
	"The above copyright notice and this permission notice shall be included in " \
	"all copies or substantial portions of the Software.\n" \
	"\n" \
	"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR " \
	"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, " \
	"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE " \
	"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER " \
	"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, " \
	"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN " \
	"THE SOFTWARE.\n" \
	"\n" \
	"The official repository for this library is at https://github.com/VA7ODR/EasyAppBase"



#include "app_logger.hpp"
#include "utils.hpp"
#include "data.hpp"
#include "eventhandler.hpp"
#include "network.hpp"
#include "thread.hpp"
#include "shared_recursive_mutex.hpp"
#include "imgui.h"
#include <SDL.h>

#define EASY_APP_VERSION_MAJOR 1
#define EASY_APP_VERSION_MINOR 0
#define EASY_APP_VERSION_PATCH 0

#define EASY_APP_VERSION_STRING "1.0.0"
#define EASY_APP_BUILD_DATE __DATE__

class EasyAppBase
{
	public:
		EasyAppBase(const std::string & sNameIn, const std::string & sTitleIn);
		EasyAppBase(std::string && sNameIn, std::string && sTitleIn);
		virtual ~EasyAppBase() = default;

		virtual void Start() {};
		virtual void Render(bool * bShow) = 0;
		virtual void Stop() {};
		virtual bool BuildsOwnWindow() { return false; }

		[[nodiscard]] const std::string & Name() const { return sName; }
		[[nodiscard]] const std::string & Title() const { return sTitle; }

		static void DisableDemo(bool bDisable);
		static void DisableDocking(bool bDisable);
		static void DisableViewports(bool bDisable);
		static void DisableGUI(bool bDisable);
		static void SetNetworkThreads(int iSetTo);

		static int Run(const std::string & sAppName, const std::string & sTitle = "");
		static void SetMainRenderer(std::function<void ()> render);

		refTSEx<json::value> ExclusiveSettings() const;

		refTSEx<json::value> SharedSettings() const;

		template <typename T>
		static std::shared_ptr<EasyAppBase> GenerateWindow()
			requires(std::derived_from<T, EasyAppBase>)
		{
			auto ret = std::make_shared<T>();
			auto registry = EasyAppBase::Registry();
			auto it = (*registry).find(ret->Name());
			if (it == (*registry).end()) {
				return (*registry)[ret->Name()] = ret;
			}
			return it->second;
		}

		static void ExitAll()
		{
			EventHandlerSet(eQuit);
		}

	protected:
		static refTSEx<std::map<std::string, std::shared_ptr<EasyAppBase>>> Registry() { return {registry, mtx}; }

	private:
		static void StartAll();
		static void Render();
		static void Menu();
		static void StopAll();

		static refTSEx<json::value> ExclusiveSettings(const std::string & sName);

		static refTSSh<json::value> SharedSettings(const std::string & sName);

		static EventHandler::Event eQuit;
		static bool bShowEasyAbout;

		static bool bDisableDemo;
		static bool bDisableDocking;
		static bool bDisableViewports;
		static bool bDisableGUI;
		static int iNetworkThreads;

		static SharedRecursiveMutex mtx;
		static json::document jSaveData;

		static SDL_Window* window;
		static std::map<std::string, std::shared_ptr<EasyAppBase>> registry;

		std::string sName;
		std::string sTitle;

		static std::function<void()> main_render;
};

class DemoWindow : public EasyAppBase
{
	public:
		DemoWindow() : EasyAppBase("demo_window", "ImGui Demo Window") {}
		bool BuildsOwnWindow() override { return true; }

		void Render(bool * bShow) override
		{
			if (*bShow) {
				ImGui::ShowDemoWindow(bShow);
			}
		}
};
