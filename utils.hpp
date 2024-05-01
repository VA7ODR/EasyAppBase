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

#include "data.hpp"
#include <boost/core/demangle.hpp>
#include <boost/stacktrace/stacktrace.hpp>
#include <functional>
#include <string>
#include <string>
#include <chrono>
#include <format>
#include"imgui.h"
#include "shared_recursive_mutex.hpp"


using namespace std::chrono_literals;

inline auto Now()
{
	return std::chrono::utc_clock::now();
}

inline auto SystemNow()
{
	return std::chrono::system_clock::now();
}

inline auto SteadyNow()
{
	return std::chrono::steady_clock::now();
}

inline std::string PrettyDateTime(auto time, const std::string_view formatIn = "{:%Y-%m-%d %H:%M:%S}")
{
	try {
		auto ret = std::vformat(formatIn, std::make_format_args(time));
		return ret;
	} catch (std::format_error & e) {
		return "Invalid time: " + std::string(e.what());
	}
}

inline std::string PrettyDate(auto time)
{
	return sPrettyDateTime(time, "{:%Y-%m-%d}");
}

inline std::string PrettyTime(auto time)
{
	return sPrettyDateTime(time, "{:%H:%M:%S}");
}

inline std::string PrettyDuration(auto duration = 0s, const std::string_view format = "{:%H:%M:%S}")
{
	return std::vformat(format, std::make_format_args(duration));
}

inline std::string PrettyDuration(auto start, auto end, const std::string_view format = "{:%H:%M:%S}")
{
	return PrettyDuration(end - start, format);
}

inline std::chrono::utc_clock::time_point UTCTime(auto time)
{
	return std::chrono::clock_cast<std::chrono::utc_clock>(time);
}

inline std::chrono::system_clock::time_point SystemTime(auto time = std::chrono::utc_clock::now())
{
	return std::chrono::clock_cast<std::chrono::system_clock>(time);
}

inline std::chrono::steady_clock::time_point SteadyTime(auto time = std::chrono::utc_clock::now())
{
	return std::chrono::clock_cast<std::chrono::steady_clock>(time);
}

inline void Asserter(bool bCondition, const std::string& sCondition, const std::string & sFile, const std::string & sFunction, int iLine) {
	if (!bCondition) {
		throw std::runtime_error(sCondition + " failed in " + sFile + " " + sFunction + " " + std::to_string(iLine));
	}
}

template <class jsonType = json::value>
void ShowJson(const std::string & sTitle, jsonType & jData, bool & bShow)
{
	static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;
	static ImGuiTreeNodeFlags tree_node_flags = ImGuiTreeNodeFlags_SpanAllColumns;

	ImGui::Text("Right-click field name or value to copy it to the clipboard.");
	if (ImGui::BeginTable(sTitle.c_str(), 3, flags)) {
		ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		std::function<void(const std::string &, jsonType &)> recursiveLambda = [&recursiveLambda](const std::string & sName, jsonType & jValue) -> void
		{
			switch (jValue.isA()) {
				case json::JSON_VOID:
					return;

				case json::JSON_NULL:
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TreeNodeEx(sName.c_str(), tree_node_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen);
					if (ImGui::IsItemClicked(1)) {
						ImGui::SetClipboardText(sName.c_str());
					}
					ImGui::TableNextColumn();
					ImGui::Text("NULL");
					if (ImGui::IsItemClicked(1)) {
						ImGui::SetClipboardText("NULL");
					}
					ImGui::TableNextColumn();
					ImGui::Text("null");
					break;

				case json::JSON_BOOLEAN:
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TreeNodeEx(sName.c_str(), tree_node_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen);
					if (ImGui::IsItemClicked(1)) {
						ImGui::SetClipboardText(sName.c_str());
					}
					ImGui::TableNextColumn();
					ImGui::Text(jValue.boolean() ? "true" : "false");
					if (ImGui::IsItemClicked(1)) {
						ImGui::SetClipboardText(jValue.boolean() ? "true" : "false");
					}
					ImGui::TableNextColumn();
					ImGui::Text("bool");
					break;

				case json::JSON_NUMBER:
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TreeNodeEx(sName.c_str(), tree_node_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen);
					if (ImGui::IsItemClicked(1)) {
						ImGui::SetClipboardText(sName.c_str());
					}
					ImGui::TableNextColumn();
					ImGui::Text("%s", jValue.c_str());
					if (ImGui::IsItemClicked(1)) {
						ImGui::SetClipboardText(jValue.c_str());
					}
					ImGui::TableNextColumn();
					ImGui::Text("number");
					break;

				case json::JSON_STRING:
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TreeNodeEx(sName.c_str(), tree_node_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen);
					if (ImGui::IsItemClicked(1)) {
						ImGui::SetClipboardText(sName.c_str());
					}
					ImGui::TableNextColumn();
					ImGui::Text("%s", jValue.c_str());
					if (ImGui::IsItemClicked(1)) {
						ImGui::SetClipboardText(jValue.c_str());
					}
					ImGui::TableNextColumn();
					ImGui::Text("string");
					break;

				case json::JSON_OBJECT:
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					bool open = ImGui::TreeNodeEx(sName.c_str(), tree_node_flags | ImGuiTreeNodeFlags_DefaultOpen);
					if (ImGui::IsItemClicked(1)) {
						ImGui::SetClipboardText(sName.c_str());
					}
					ImGui::TableNextColumn();
					ImGui::TextDisabled("--");
					ImGui::TableNextColumn();
					ImGui::Text("object");
					if (open) {
						for (auto & sub : jValue) {
							recursiveLambda(sub.key(), sub);
						}
						ImGui::TreePop();
					}
					break;
				}

				case json::JSON_ARRAY:
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					bool open = ImGui::TreeNodeEx(sName.c_str(), tree_node_flags | ImGuiTreeNodeFlags_DefaultOpen);
					if (ImGui::IsItemClicked(1)) {
						ImGui::SetClipboardText(sName.c_str());
					}
					ImGui::TableNextColumn();
					ImGui::TextDisabled("--");
					ImGui::TableNextColumn();
					ImGui::Text("array");
					if (open) {
						size_t iIndex = 0;
						for (auto & sub : jValue) {
							recursiveLambda(std::to_string(iIndex++), sub);
						}
						ImGui::TreePop();
					}
					break;
				}
			}
		};

		recursiveLambda(sTitle.c_str(), jData);
		ImGui::EndTable();
	}
}

void ShowJsonWindow(const std::string & sTitle, json::value & jData, bool & bShow);
std::string PrettyHex(const std::vector<unsigned char> &in);

template <class jsonType = json::value>
jsonType ImVec4ToJSONArray(const ImVec4 & in)
{
	jsonType ret;
	ret[0] = in.x;
	ret[1] = in.y;
	ret[2] = in.z;
	ret[3] = in.w;
	return ret;
}

template <class jsonType = json::value>
ImVec4 JSONArrayToImVec4(jsonType & jData)
{
	ImVec4 ret {0, 0, 0, 0};

	if (jData.IsArray()) {
		ret.x = jData[0]._float();
		ret.y = jData[1]._float();
		ret.z = jData[2]._float();
		ret.w = jData[3]._float();
	}
	return ret;
}

template <typename T, class jsonType = json::value>
class jsonTypedRef
{
	public:
		jsonTypedRef(jsonType & jValIn)
			requires std::is_same_v<T, bool>
			: jVal(jValIn)
		{
			m_val = jVal.boolean();
		}

		jsonTypedRef(jsonType & jValIn)
			requires std::is_floating_point_v<T>
			: jVal(jValIn)
		{
			m_val = jVal._double();
		}

		jsonTypedRef(jsonType & jValIn)
			requires std::is_integral_v<T> && std::is_signed_v<T> && (!std::is_same_v<T, bool>)
			: jVal(jValIn)
		{
			m_val = jVal._int64();
		}

		jsonTypedRef(jsonType & jValIn)
			requires std::is_integral_v<T> && std::is_unsigned_v<T> && (!std::is_same_v<T, bool>)
			: jVal(jValIn)
		{
			m_val = jVal._uint64();
		}

		jsonTypedRef(jsonType & jValIn)
			requires std::is_same_v<T, char *> || std::is_same_v<T, std::string> || std::is_same_v<T, sdstring>
			: jVal(jValIn)
		{
			m_val = jVal.string().data();
		}

		~jsonTypedRef() { jVal = m_val; }

		operator T *()
			requires(!std::is_pointer_v<T>)
		{
			return &m_val;
		}

		operator T()
			requires(std::is_pointer_v<T>)
		{
			return m_val;
		}

	private:
		T m_val;
		jsonType & jVal;
};

template <typename T, class jsonType = json::value>
class jsonTypedRefTSSh : public jsonTypedRef<T, jsonType>
{
	public:
		jsonTypedRefTSSh(jsonType & jValIn, SharedRecursiveMutex & mtxIn) :
			jsonTypedRef<T, jsonType>(jValIn),
			lock(mtxIn)
		{

		}

	private:
		RecursiveSharedLock lock;
};

template <typename T, class jsonType = json::value>
class jsonTypedRefTSEx : public jsonTypedRef<T, jsonType>
{
	public:
		jsonTypedRefTSEx(jsonType & jValIn, SharedRecursiveMutex & mtxIn) :
			jsonTypedRef<T, jsonType>(jValIn),
			lock(mtxIn)
		{

		}

	private:
		RecursiveExclusiveLock lock;
};

template <class T = json::value>
class refTSSh
{
	public:
		refTSSh(T & in, SharedRecursiveMutex & mtxIn) :
			lock(mtxIn),
			ref(in)
		{

		}

		operator T *()
			requires(!std::is_pointer_v<T>)
		{
			return &ref;
		}

		operator T()
			requires(std::is_pointer_v<T>)
		{
			return ref;
		}

	private:
		RecursiveSharedLock lock;
		T & ref;
};

template <class T = json::value>
class refTSEx
{
	public:
		refTSEx(T & in, SharedRecursiveMutex & mtxIn) :
			lock(mtxIn),
			ref(in)
		{

		}

		operator T *()
			requires(!std::is_pointer_v<T>)
		{
			return &ref;
		}

		operator T()
			requires(std::is_pointer_v<T>)
		{
			return ref;
		}

	private:
		RecursiveExclusiveLock lock;
		T & ref;
};



#define ASSERT(bCondition) Asserter(bCondition, #bCondition, SOURCE_FILE, __FUNCTION__, __LINE__)
#define SOURCE_FILE std::string(__FILE__).substr(strlen(SOURCE_DIR) + 1)


std::string GetAppDataFolder();
std::string PrettyHex(const std::vector<unsigned char> &in);
