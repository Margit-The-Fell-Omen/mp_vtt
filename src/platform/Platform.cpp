#include "platform/Platform.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

namespace ab {
namespace platform {

namespace {

std::string g_androidDataDir;
std::string g_bundledFontDir;

#if !defined(_WIN32) && !defined(__ANDROID__)
std::string shellSingleQuote(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('\'');
    for (char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('\'');
    return out;
}
#endif

#if defined(_WIN32)
std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int needed = MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(),
                        static_cast<int>(text.size()), out.data(), needed);
    return out;
}

std::string wideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int needed = WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
        nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(),
                        static_cast<int>(text.size()), out.data(), needed,
                        nullptr, nullptr);
    return out;
}
#endif

}  // namespace

std::string homeDir() {
#if defined(__ANDROID__)
    if (!g_androidDataDir.empty()) {
        return g_androidDataDir;
    }
#endif
#if defined(_WIN32)
    wchar_t buffer[32768];
    const DWORD len =
        GetEnvironmentVariableW(L"USERPROFILE", buffer, 32768);
    if (len > 0 && len < 32768) {
        return wideToUtf8(std::wstring(buffer, len));
    }
#endif
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return home;
    }
    const char* profile = std::getenv("USERPROFILE");
    if (profile && *profile) {
        return profile;
    }
    return ".";
}

void setAndroidDataDir(const std::string& path) {
    g_androidDataDir = path;
}

void setBundledFontDir(const std::string& path) {
    g_bundledFontDir = path;
}

std::string bundledFontDir() {
    return g_bundledFontDir;
}

std::filesystem::path configDir() {
    if (!g_androidDataDir.empty()) {
        return pathFromUtf8(g_androidDataDir);
    }
    return pathFromUtf8(homeDir()) / ".adventureboard";
}

std::string pathToUtf8(const std::filesystem::path& path) {
    return path.u8string();
}

std::filesystem::path pathFromUtf8(const std::string& text) {
    return std::filesystem::u8path(text);
}

std::vector<std::filesystem::path> rootLocations() {
    std::vector<std::filesystem::path> roots;
#if defined(_WIN32)
    const DWORD mask = GetLogicalDrives();
    for (DWORD i = 0; i < 26; ++i) {
        if ((mask & (1u << i)) == 0) {
            continue;
        }
        wchar_t letter = static_cast<wchar_t>(L'A' + i);
        roots.emplace_back(std::wstring{letter, L':', L'\\'});
    }
#endif
    return roots;
}

bool readFileBytes(const std::filesystem::path& path,
                   std::vector<unsigned char>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in),
               std::istreambuf_iterator<char>());
    return !in.bad();
}

bool openUrl(const std::string& url) {
    if (url.empty()) {
        return false;
    }
#if defined(__ANDROID__)
    (void)url;  // Intent bridge is added with the SAF/JNI host.
    return false;
#elif defined(_WIN32)
    const std::wstring wide = utf8ToWide(url);
    if (wide.empty()) {
        return false;
    }
    const HINSTANCE result = ShellExecuteW(
        nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
#else
    const char* opener = "xdg-open";
#if defined(__APPLE__)
    opener = "open";
#endif
    const std::string cmd = std::string(opener) + " " +
                            shellSingleQuote(url) + " >/dev/null 2>&1 &";
    return std::system(cmd.c_str()) == 0;
#endif
}

}  // namespace platform
}  // namespace ab
