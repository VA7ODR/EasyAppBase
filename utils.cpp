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

The official repository for this library is at https://github.com/VA7ODR/EasyAppBase

*/

#include "utils.hpp"
#include <string>
#include <vector>

std::string PrettyHex(const std::vector<unsigned char> &in)
{
	std::string sRet;
	sRet.reserve(in.size() * 8);
	for (auto & c : in) {
		if (c < ' ' || c >= 127) {
			char szHex[24];
			sprintf(szHex, "[%.2X]", c);
			sRet.append(szHex);
		} else {
			sRet.push_back((char)c);
		}
	}
	return sRet;
}

std::string GetAppDataFolder()
{
#if defined(_WIN32)
	const char *appDataEnvVar = std::getenv("APPDATA");
	if (appDataEnvVar)
		return appDataEnvVar + "/";
	else
		return "./"; // Handle error on Windows
#elif defined(__APPLE__)
	const char *homeDir = std::getenv("HOME");
	if (homeDir)
		return std::string(homeDir) + "/Library/Application Support/";
	else
		return "./"; // Handle error on macOS
#else
	const char *homeDir = std::getenv("HOME");
	if (homeDir)
		return std::string(homeDir) + "/.local/share/";
	else
		return "./"; // Handle error on Linux
#endif
}



