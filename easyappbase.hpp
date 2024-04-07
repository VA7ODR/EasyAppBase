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
#include "app_logger.hpp"
//#include "config.hpp"
#include "utils.hpp"
#include "data.hpp"
#include "eventhandler.hpp"
#include "network.hpp"
#include "thread.hpp"
#include "shared_recursive_mutex.hpp"
#include "imgui.h"
#include <SDL.h>
#include "config.hpp"

class EasyAppBase
{
	public:
		EasyAppBase(const std::string & sNameIn, const std::string sTitleIn);

		virtual void Start() {};
		virtual void Render(bool * bShow) = 0;
		virtual void Stop() {};
		virtual bool BuildsOwnWindow() { return false; }

		const std::string & Name() const { return sName; }
		const std::string & Title() const { return sTitle; }

		static int Run();
		static void Render();

		refTSEx<json::value> ExclusiveSettings();

		refTSEx<json::value> SharedSettings();

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

	protected:
		static refTSEx<std::map<std::string, std::shared_ptr<EasyAppBase>>> Registry() { return {registry, mtx}; }

	private:
		static void Init();
		static void Menu();
		static void StartAll();
		static void StopAll();

		static refTSEx<json::value> ExclusiveSettings(const std::string & sName);

		static refTSSh<json::value> SharedSettings(const std::string & sName);

		static bool bQuit;

		static SharedRecursiveMutex mtx;
		static json::document jSaveData;

		static SDL_Window* window;
		static std::map<std::string, std::shared_ptr<EasyAppBase>> registry;

		std::string sName;
		std::string sTitle;
};

#define INIT_HEADER void EasyAppBase::Init() {
#define ADD_WINDOW(X) GenerateWindow<X>()
#define INIT_FOOTER }



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

