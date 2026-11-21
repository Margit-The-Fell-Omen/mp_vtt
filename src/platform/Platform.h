#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ab {
namespace platform {

// User's home directory. Returns "." when no home-like variable is set.
std::string homeDir();

// Android has no home directory; the NativeActivity host sets this before
// core initialization. Ignored on desktop.
void setAndroidDataDir(const std::string& path);

// Directory containing bundled TTF files. Empty means "use AB_ASSET_DIR".
void setBundledFontDir(const std::string& path);
std::string bundledFontDir();

// Application data directory (currently ~/.adventureboard on every desktop).
std::filesystem::path configDir();

// UTF-8 conversion helpers for paths crossing the database/UI boundary.
std::string pathToUtf8(const std::filesystem::path& path);
std::filesystem::path pathFromUtf8(const std::string& text);

// Filesystem roots the folder picker may offer. On Windows this is one entry
// per mounted drive; on other platforms it may be empty.
std::vector<std::filesystem::path> rootLocations();

// Reads a whole file into memory. Returns false on failure.
bool readFileBytes(const std::filesystem::path& path,
                   std::vector<unsigned char>& out);

// Opens a URL or local attachment in the system handler. Returns false when
// the platform-specific call could not be started.
bool openUrl(const std::string& url);

}  // namespace platform
}  // namespace ab
