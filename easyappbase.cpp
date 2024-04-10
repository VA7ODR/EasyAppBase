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

EventHandler::Event EasyAppBase::eQuit = EventHandler::CreateEvent("Application Quit", EventHandler::manual_reset);
bool EasyAppBase::bShowEasyAbout = false;
bool EasyAppBase::bDisableDemo = false;
bool EasyAppBase::bDisableDocking = false;
bool EasyAppBase::bDisableViewports = false;
bool EasyAppBase::bDisableGUI = false;
int EasyAppBase::iNetworkThreads = 0;

std::function<void()> EasyAppBase::main_render = nullptr;

SharedRecursiveMutex EasyAppBase::mtx;
json::document EasyAppBase::jSaveData;
SDL_Window* EasyAppBase::window = nullptr;
std::map<std::string, std::shared_ptr<EasyAppBase>> EasyAppBase::registry;

EasyAppBase::EasyAppBase(const std::string & sNameIn, const std::string sTitleIn) : sName(sNameIn), sTitle(sTitleIn)
{

}

void EasyAppBase::DisableDemo(bool bDisable)
{
	bDisableDemo = bDisable;
}

void EasyAppBase::DisableDocking(bool bDisable)
{
	bDisableDocking = bDisable;
}

void EasyAppBase::DisableViewports(bool bDisable)
{
	bDisableViewports = bDisable;
}

void EasyAppBase::DisableGUI(bool bDisable)
{
	bDisableGUI = bDisable;
}

void EasyAppBase::SetNetworkThreads(int iSetTo)
{
	iNetworkThreads = iSetTo;
}

int EasyAppBase::Run(const std::string & sAppName, const std::string & sTitle)
{
	if (bDisableGUI) {
		AppLogger::CloneToCout(true);
	}
	std::string sWindowTitle;
	if (sTitle.size()) {
		sWindowTitle = sTitle;
	} else {
		sWindowTitle = sAppName;
	}
	AppLogger::CloneToCout(true);
	if (!std::filesystem::exists(GetAppDataFolder() + sAppName)) {
		std::error_code ec;
		std::filesystem::create_directories(GetAppDataFolder() + sAppName, ec);
		if (ec) {
			Log(AppLogger::ERROR) << "Failed to create save data folder: " << GetAppDataFolder() << sAppName << ".\n\t" << ec.message();
		}
	}

	{
		RecursiveExclusiveLock lock(mtx);
		if (jSaveData.parseFile(GetAppDataFolder() + sAppName + "/settings.json")) {
			Log(AppLogger::INFO) << "Opened settings: " << GetAppDataFolder() << sAppName << "/settings.json";
		} else {
			Log(AppLogger::WARNING) << "Failed toopen settings: " << GetAppDataFolder() << sAppName << "/settings.json";
		}
	}

	Network::Core(iNetworkThreads);

	if (bDisableGUI) {
		EventHandlerWait({eQuit}, EventHandler::INFINITE);
	} else {
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

		uint32_t iX = SDL_WINDOWPOS_CENTERED;
		uint32_t iY = SDL_WINDOWPOS_CENTERED;
		uint32_t iW = 1280;
		uint32_t iH = 720;

		SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
		{
			RecursiveSharedLock lock(mtx);
			if (jSaveData["main_window"].exists("x")) {
				iX = jSaveData["main_window"]["x"]._uint32();
			}
			if (jSaveData["main_window"].exists("y")) {
				iY = jSaveData["main_window"]["y"]._uint32();
			}
			if (jSaveData["main_window"].exists("w")) {
				iW = jSaveData["main_window"]["w"]._uint32();
			}
			if (jSaveData["main_window"].exists("h")) {
				iH = jSaveData["main_window"]["h"]._uint32();
			}
			if (jSaveData["main_window"]["fullscreen"].boolean()) {
				window_flags = (SDL_WindowFlags)(window_flags | SDL_WINDOW_FULLSCREEN_DESKTOP);
			} else if (jSaveData["main_window"]["state"] == "maximized") {
				window_flags = (SDL_WindowFlags)(window_flags | SDL_WINDOW_MAXIMIZED);
			} else if (jSaveData["main_window"]["state"] == "minimized") {
				window_flags = (SDL_WindowFlags)(window_flags | SDL_WINDOW_MINIMIZED);
			}
		}

		window = SDL_CreateWindow(sWindowTitle.c_str(), iX, iY, iW, iH, window_flags);
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
		std::string sIniFileName = GetAppDataFolder() + sAppName + "/imgui.ini";
		io.IniFilename = sIniFileName.c_str();
		Log(AppLogger::DEBUG) << "Set ImGui ini file to: " << sIniFileName;

		io.Fonts->AddFontFromMemoryCompressedTTF(HackFont_compressed_data, HackFont_compressed_size, 12);
		io.Fonts->AddFontFromMemoryCompressedTTF(HackFont_compressed_data, HackFont_compressed_size, 18);
		io.Fonts->AddFontFromMemoryCompressedTTF(HackFont_compressed_data, HackFont_compressed_size, 27);
		io.Fonts->AddFontFromMemoryCompressedTTF(HackFont_compressed_data, HackFont_compressed_size, 36);
		io.Fonts->AddFontFromMemoryCompressedTTF(HackFont_compressed_data, HackFont_compressed_size, 45);

		if (!jSaveData.exists("font_size")) {
			jSaveData["font_size"] = 1;
		}

		io.FontDefault = io.Fonts->Fonts[*jsonTypedRefTSSh<int>(jSaveData["font_size"], mtx)];

		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		if (!bDisableDocking) {
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
		}
		if (!bDisableViewports) {
			io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
		}
		//io.ConfigViewportsNoAutoMerge = true;
		//io.ConfigViewportsNoTaskBarIcon = true;

		// Setup Dear ImGui style
		{
			RecursiveSharedLock lock(mtx);
			switch(jSaveData["style"]._int()) {
				case 0:
				default:
					ImGui::StyleColorsDark();
					break;

				case 1:
					ImGui::StyleColorsLight();
					break;

				case 3:
					ImGui::StyleColorsClassic();
					break;
			}
		}

		//ImGui::StyleColorsLight();

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& style = ImGui::GetStyle();

		style.DisplayWindowPadding = {0, 0};

		style.WindowRounding = 8.0;
		style.ChildRounding = 8.0;
		style.FrameRounding = 8.0;
		style.PopupRounding = 8.0;
		style.ScrollbarRounding = 8.0;
		style.GrabRounding = 8.0;
		style.TabRounding = 6.0;

		style.WindowBorderSize = 1.0;
		style.ChildBorderSize = 1.0;
		style.PopupBorderSize = 1.0;
		style.FrameBorderSize = 1.0;
		style.TabBorderSize = 1.0;

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		// Setup Platform/Renderer backends
		ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
		ImGui_ImplOpenGL3_Init(glsl_version);

		// Our state
		ImVec4 clear_color = ImVec4(0.0, 0.0, 0.0, 1.0);

		if (!bDisableDemo) {
			// Add Demo Window
			auto demoWindow = GenerateWindow<DemoWindow>();
		}

		StartAll();

		// ImGui::LoadIniSettingsFromDisk(io.IniFilename);

	#ifdef __EMSCRIPTEN__
		// For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
		// You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
		io.IniFilename = nullptr;
		EMSCRIPTEN_MAINLOOP_BEGIN
			#else
		while (EventHandlerWait({eQuit}, 0ms) == EventHandler::TIMEOUT)
	#endif
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				ImGui_ImplSDL2_ProcessEvent(&event);

				if (event.type == SDL_QUIT) {
					ExitAll();
				} else if (event.type == SDL_WINDOWEVENT && event.window.windowID == SDL_GetWindowID(window)) {
					switch (event.window.event) {
						case SDL_WINDOWEVENT_CLOSE:
							ExitAll();
							break;

						case SDL_WINDOWEVENT_MOVED:
						{
							RecursiveExclusiveLock lock(mtx);
							jSaveData["main_window"]["x"] = event.window.data1;
							jSaveData["main_window"]["y"] = event.window.data2;
							break;
						}

						case SDL_WINDOWEVENT_SIZE_CHANGED:
						case SDL_WINDOWEVENT_RESIZED:
						{
							RecursiveExclusiveLock lock(mtx);
							jSaveData["main_window"]["w"] = event.window.data1;
							jSaveData["main_window"]["h"] = event.window.data2;
							break;
						}

						case SDL_WINDOWEVENT_MAXIMIZED:
						{
							RecursiveExclusiveLock lock(mtx);
							jSaveData["main_window"]["state"] = "maximized";
							break;
						}

						case SDL_WINDOWEVENT_MINIMIZED:
						{
							RecursiveExclusiveLock lock(mtx);
							jSaveData["main_window"]["state"] = "minimized";
							break;
						}

						case SDL_WINDOWEVENT_RESTORED:
						{
							RecursiveExclusiveLock lock(mtx);
							SDL_SetWindowFullscreen(window, 0);
							SDL_SetWindowPosition(window, jSaveData["main_window"]["x"]._uint32(), jSaveData["main_window"]["y"]._uint32());
							SDL_SetWindowSize(window, jSaveData["main_window"]["w"]._uint32(), jSaveData["main_window"]["h"]._uint32());
							jSaveData["main_window"]["state"] = "";
							break;
						}
					}
				} else {
					switch (event.type) {
						case SDL_KEYDOWN:
						{

							break;
						}
						case SDL_KEYUP:
						{
							switch (event.key.type) {

							}

							break;
						}
					}
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

	#ifdef __EMSCRIPTEN__
		EMSCRIPTEN_MAINLOOP_END;
	#endif

		StopAll();

		// Cleanup
		ImGui::SaveIniSettingsToDisk(io.IniFilename);
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();

		SDL_GL_DeleteContext(gl_context);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}

	Network::ExitAll();
	EventHandler::ExitAll();

	RecursiveExclusiveLock lock(mtx);
	if (jSaveData.writeFile(GetAppDataFolder() + sAppName + "/settings.json", true)) {
		Log(AppLogger::INFO) << "Saved settings: " << GetAppDataFolder() << sAppName << "/settings.json";
	} else {
		Log(AppLogger::WARNING) << "Failed to save settings: " << GetAppDataFolder() << sAppName << "/settings.json";
	}

	return 0;
}

void EasyAppBase::Render()
{
	ImGuiIO& io = ImGui::GetIO();
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	Menu();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("##Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
	ImGui::PopStyleVar(2);

	if (main_render) {
		main_render();
	}

	ImGuiID dockspace_id = 0;
	if (bShowEasyAbout) {
		ImGui::OpenPopup("About EasyAppBase");
		auto size = viewport->Size;
		size.x = size.x * 0.5;
		size.y = size.y * 0.5;
		auto pos = viewport->Pos;
		pos.x = pos.x + (viewport->Size.x - size.x) * 0.5;
		pos.y = pos.y + (viewport->Size.y - size.y) * 0.5;

		ImGui::SetNextWindowPos(pos);
		ImGui::SetNextWindowSize(size);
		bShowEasyAbout = false;
	}

	if (ImGui::BeginPopupModal("About EasyAppBase", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::PushFont(io.Fonts->Fonts[4]);
		ImGui::Text("EasyAppBase v%s (%s)", EASY_APP_VERSION_STRING, EASY_APP_BUILD_DATE);
		ImGui::PopFont();
		ImGui::BeginChild("##EasyLicence", {0, 0}, ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY);
		ImGui::TextWrapped(EASY_APP_LICENSE);
		ImGui::EndChild();
		ImGui::Text("This application is using:");
		ImGui::Indent();

		if (ImGui::BeginTable("##EasyAppDepends", 3)) {
			ImGui::TableSetupColumn("Library", ImGuiTableColumnFlags_NoHide);
			ImGui::TableSetupColumn("Version", ImGuiTableColumnFlags_NoHide);
			ImGui::TableSetupColumn("URL", ImGuiTableColumnFlags_NoHide);
			ImGui::TableHeadersRow();

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("SDL2");
			ImGui::TableNextColumn();
			ImGui::Text("%s", SDL_GetRevision());
			ImGui::TableNextColumn();
			ImGui::Text("https://www.libsdl.org/");

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("ImGui");
			ImGui::TableNextColumn();
			ImGui::Text("%s", IMGUI_VERSION);
			ImGui::TableNextColumn();
			ImGui::Text("https://github.com/ocornut/imgui");

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("json_document");
			ImGui::TableNextColumn();
			ImGui::Text("%s", JSON_DOCUMENT_VERSION);
			ImGui::TableNextColumn();
			ImGui::Text("https://github.com/VA7ODR/json_document");

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("SDString");
			ImGui::TableNextColumn();
			ImGui::Text("%s", JSON_DOCUMENT_VERSION);
			ImGui::TableNextColumn();
			ImGui::Text("https://github.com/VA7ODR/SDString");

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Arbitrary Order Map");
			ImGui::TableNextColumn();
			ImGui::Text("%s", JSON_DOCUMENT_VERSION);
			ImGui::TableNextColumn();
			ImGui::Text("https://github.com/VA7ODR/ArbitraryOrderMap");

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Boost");
			ImGui::TableNextColumn();
			ImGui::Text("%s", BOOST_LIB_VERSION);
			ImGui::TableNextColumn();
			ImGui::Text("https://www.boost.org/");

			const GLubyte* version = glGetString(GL_VERSION);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("OpenGL");
			ImGui::TableNextColumn();
			ImGui::Text("%s", version);
			ImGui::TableNextColumn();
			ImGui::Text("https://www.opengl.org/");

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("OpenSSL");
			ImGui::TableNextColumn();
			ImGui::Text("%s", OPENSSL_FULL_VERSION_STR);
			ImGui::TableNextColumn();
			ImGui::Text("https://www.openssl.org/");


			ImGui::EndTable();
		}



		ImGui::Unindent();
		if (ImGui::Button("Close")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	auto & registry = *EasyAppBase::Registry();
	if (registry.size()) {
		if (!bDisableDocking) {
			ImGui::BeginChild("MyDockSpace", ImGui::GetContentRegionAvail(), ImGuiChildFlags_FrameStyle);
			if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
				dockspace_id = ImGui::GetID("MyDockSpace");
				ImGui::DockSpace(dockspace_id, ImGui::GetContentRegionAvail(), ImGuiDockNodeFlags_None);
			}
		}
		for (auto & window : registry) {
			bool bShow = jSaveData["show"][window.first].boolean();
			if (window.second->BuildsOwnWindow()) {
				window.second->Render(&bShow);
			} else {
				// Next window should start docked to the main window.
				ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 650, viewport->WorkPos.y + 20), ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);
				if (!bDisableDocking) {
					ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
				}
				if (bShow) {
					ImGui::Begin(window.second->Title().c_str(), &bShow);
					window.second->Render(&bShow);
					ImGui::End();
				}
			}
			jSaveData["show"][window.first] = bShow;
		}
		if (!bDisableDocking) {
			ImGui::EndChild();
		}
	}
	ImGui::End();
}

void EasyAppBase::SetMainRenderer(std::function<void ()> render)
{
	RecursiveExclusiveLock lock(mtx);
	main_render = render;
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
	ImGui::BeginMainMenuBar();

	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Quit")) {
			ExitAll();
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("View")) {
		RecursiveExclusiveLock lock(mtx);

		if (ImGui::MenuItem("Fullscreen", "F11", jSaveData["main_window"]["fullscreen"].boolean())) {
			jSaveData["main_window"]["fullscreen"] = !jSaveData["main_window"]["fullscreen"].boolean();
			if (jSaveData["main_window"]["fullscreen"].boolean()) {
				SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
			} else {
				SDL_SetWindowFullscreen(window, 0);
				SDL_SetWindowPosition(window, jSaveData["main_window"]["x"]._uint32(), jSaveData["main_window"]["y"]._uint32());
				SDL_SetWindowSize(window, jSaveData["main_window"]["w"]._uint32(), jSaveData["main_window"]["h"]._uint32());
			}
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Scale: Extra Small", nullptr, jSaveData["font_size"] == 0)) {
			ImGuiIO& io = ImGui::GetIO(); (void)io;
			io.FontDefault = io.Fonts->Fonts[0];
			jSaveData["font_size"] = 0;
		}
		if (ImGui::MenuItem("Scale: Small", nullptr, jSaveData["font_size"] == 1)) {
			ImGuiIO& io = ImGui::GetIO(); (void)io;
			io.FontDefault = io.Fonts->Fonts[1];
			jSaveData["font_size"] = 1;
		}
		if (ImGui::MenuItem("Scale: Default", nullptr, jSaveData["font_size"] == 2)) {
			ImGuiIO& io = ImGui::GetIO(); (void)io;
			io.FontDefault = io.Fonts->Fonts[2];
			jSaveData["font_size"] = 2;
		}
		if (ImGui::MenuItem("Scale: Large", nullptr, jSaveData["font_size"] == 3)) {
			ImGuiIO& io = ImGui::GetIO(); (void)io;
			io.FontDefault = io.Fonts->Fonts[3];
			jSaveData["font_size"] = 3;
		}
		if (ImGui::MenuItem("Scale: Extra Large", nullptr, jSaveData["font_size"] == 4)) {
			ImGuiIO& io = ImGui::GetIO(); (void)io;
			io.FontDefault = io.Fonts->Fonts[4];
			jSaveData["font_size"] = 4;
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Style: Dark", nullptr, jSaveData["style"] == 0)) {
			ImGuiIO& io = ImGui::GetIO(); (void)io;
			ImGui::StyleColorsDark();
			jSaveData["style"] = 0;
		}
		if (ImGui::MenuItem("Style: Light", nullptr, jSaveData["style"] == 1)) {
			ImGuiIO& io = ImGui::GetIO(); (void)io;
			ImGui::StyleColorsLight();
			jSaveData["style"] = 1;
		}
		if (ImGui::MenuItem("Style: Dark", nullptr, jSaveData["style"] == 2)) {
			ImGuiIO& io = ImGui::GetIO(); (void)io;
			ImGui::StyleColorsClassic();
			jSaveData["style"] = 2;
		}

		ImGui::End();
	}

	{
		auto & registry = *EasyAppBase::Registry();
		if (registry.size()) {
			if (ImGui::BeginMenu("Windows"))	{
				for (auto & window : registry) {
					bool bShow = jSaveData["show"][window.first].boolean();
					if (ImGui::MenuItem(window.second->Name().c_str(), "", bShow)) {
						bShow = !bShow;
					}
					jSaveData["show"][window.first] = bShow;
				}
				ImGui::EndMenu();
			}
		}
	}

	if (ImGui::BeginMenu("Help"))	{
		auto & registry = *EasyAppBase::Registry();
		if (ImGui::MenuItem("About EasyAppBase...", "")) {
			bShowEasyAbout = true;
		}
		for (auto & window : registry) {
			if (ImGui::MenuItem(("About " + window.second->Title() + "...").c_str(), "")) {
				bShowEasyAbout = true;
			}
		}
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();
}


refTSSh<json::value> EasyAppBase::SharedSettings(const std::string & sName)
{
	return {jSaveData["sub"][sName], mtx};
}

refTSEx<json::value> EasyAppBase::ExclusiveSettings(const std::string & sName)
{
	return {jSaveData["sub"][sName], mtx};
}
