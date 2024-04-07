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

#include "easyappbase.hpp"
#include <filesystem>

using namespace std::chrono_literals;

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>

extern const unsigned int HackFont_compressed_size;
extern const unsigned int HackFont_compressed_data[];

bool EasyAppBase::bQuit = false;
SharedRecursiveMutex EasyAppBase::mtx;
json::document EasyAppBase::jSaveData;
SDL_Window* EasyAppBase::window = nullptr;
std::map<std::string, std::shared_ptr<EasyAppBase>> EasyAppBase::registry;

EasyAppBase::EasyAppBase(const std::string & sNameIn, const std::string sTitleIn) : sName(sNameIn), sTitle(sTitleIn)
{

}

int EasyAppBase::Run()
{
	AppLogger::CloneToCout(true);
	if (!std::filesystem::exists(GetAppDataFolder() + APP_NAME)) {
		std::error_code ec;
		std::filesystem::create_directories(GetAppDataFolder() + APP_NAME, ec);
		if (ec) {
			Log(AppLogger::ERROR) << "Failed to create save data folder: " << GetAppDataFolder() << APP_NAME << ".\n\t" << ec.message();
		}
	}

	{
		RecursiveExclusiveLock lock(mtx);
		if (jSaveData.parseFile(GetAppDataFolder() + APP_NAME + "/settings.json")) {
			Log(AppLogger::INFO) << "Opened settings: " << GetAppDataFolder() << APP_NAME << "/settings.json";
		} else {
			Log(AppLogger::WARNING) << "Failed toopen settings: " << GetAppDataFolder() << APP_NAME << "/settings.json";
		}
	}

	// Setup SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
	{
		printf("Error: %s\n", SDL_GetError());
		return -1;
	}

	// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
	// GL ES 2.0 + GLSL 100
	const char* glsl_version = "#version 100";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
	// GL 3.2 Core + GLSL 150
	const char* glsl_version = "#version 150";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
	// GL 3.0 + GLSL 130
	const char* glsl_version = "#version 130";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

	// From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

	// Create window with graphics context
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	window = SDL_CreateWindow(WINDOW_TITLE " (" __DATE__ ")", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
	if (window == nullptr)
	{
		printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
		return -1;
	}

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, gl_context);
	SDL_GL_SetSwapInterval(1); // Enable vsync

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = (GetAppDataFolder() + APP_NAME + "/imgui.ini").c_str();
	Log(AppLogger::DEBUG) << "Set ImGui ini file to: " << GetAppDataFolder() << APP_NAME << "/imgui.ini";

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
	//io.ConfigViewportsNoAutoMerge = true;
	//io.ConfigViewportsNoTaskBarIcon = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init(glsl_version);

	io.Fonts->AddFontFromMemoryCompressedTTF(HackFont_compressed_data, HackFont_compressed_size, 12);
	io.Fonts->AddFontFromMemoryCompressedTTF(HackFont_compressed_data, HackFont_compressed_size, 18);
	io.Fonts->AddFontFromMemoryCompressedTTF(HackFont_compressed_data, HackFont_compressed_size, 27);
	io.Fonts->AddFontFromMemoryCompressedTTF(HackFont_compressed_data, HackFont_compressed_size, 36);

	// Our state
	ImVec4 clear_color = ImVec4(0.0, 0.0, 0.0, 1.0);

	Init();

#ifdef __EMSCRIPTEN__
	// For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
	// You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
	io.IniFilename = nullptr;
	EMSCRIPTEN_MAINLOOP_BEGIN
		#else
	while (!bQuit)
#endif
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);

			if (event.type == SDL_QUIT) {
				bQuit = true;
			}
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) {
				bQuit = true;
			}
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		Render();

		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
			SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
		}

		SDL_GL_SwapWindow(window);
	}

	StopAll();

#ifdef __EMSCRIPTEN__
	EMSCRIPTEN_MAINLOOP_END;
#endif

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	RecursiveExclusiveLock lock(mtx);
	jSaveData.writeFile(GetAppDataFolder() + APP_NAME + "/settings.json", true);

	return 0;
}

void EasyAppBase::Render()
{
	ImGuiIO& io = ImGui::GetIO();
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("##Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar);
	ImGui::PopStyleVar(2);
	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
	}
	Menu();
	auto & registry = *EasyAppBase::Registry();
	for (auto & window : registry) {
		bool bShow = jSaveData["show"][window.first].boolean();
		if (window.second->BuildsOwnWindow()) {
			window.second->Render(&bShow);
		} else {
			if (ImGui::Begin(window.second->Title().c_str(), &bShow)) {
				window.second->Render(&bShow);
				ImGui::End();
			}
		}
		jSaveData["show"][window.first] = bShow;
	}
	ImGui::End();
}

refTSEx<json::value> EasyAppBase::ExclusiveSettings()
{
	return ExclusiveSettings(sName);
}

refTSEx<json::value> EasyAppBase::SharedSettings()
{
	return ExclusiveSettings(sName);
}

void EasyAppBase::StopAll()
{
	auto & registry = *EasyAppBase::Registry();
	for (auto & window : registry) {
		window.second->Stop();
	}
}

void EasyAppBase::StartAll()
{
	auto & registry = *EasyAppBase::Registry();
	for (auto & window : registry) {
		window.second->Start();
	}
}

void EasyAppBase::Menu()
{
	ImGui::BeginMenuBar();

	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Quit")) {
			bQuit = true;
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Windows"))	{
		auto & registry = *EasyAppBase::Registry();
		for (auto & window : registry) {
			bool bShow = jSaveData["show"][window.first].boolean();
			if (ImGui::MenuItem(window.second->Name().c_str(), "", bShow)) {
				bShow = !bShow;
			}
			jSaveData["show"][window.first] = bShow;
		}
		ImGui::EndMenu();
	}


	ImGui::EndMenuBar();
}


refTSSh<json::value> EasyAppBase::SharedSettings(const std::string & sName)
{
	return {jSaveData["sub"][sName], mtx};
}

refTSEx<json::value> EasyAppBase::ExclusiveSettings(const std::string & sName)
{
	return {jSaveData["sub"][sName], mtx};
}
