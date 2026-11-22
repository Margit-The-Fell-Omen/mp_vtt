#include "platform/desktop/DesktopStorage.h"

#include "platform/Platform.h"

#include <fstream>
#include <sstream>

namespace ab {

namespace fs = std::filesystem;

bool DesktopStorage::exists(const Path& location) const {
    std::error_code ec;
    return fs::exists(location, ec) && !ec;
}

bool DesktopStorage::isDirectory(const Path& location) const {
    std::error_code ec;
    return fs::is_directory(location, ec) && !ec;
}

bool DesktopStorage::isRegularFile(const Path& location) const {
    std::error_code ec;
    return fs::is_regular_file(location, ec) && !ec;
}

bool DesktopStorage::readText(const Path& location, std::string& out) const {
    std::ifstream in(location, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool DesktopStorage::writeText(const Path& location,
                               const std::string& content) {
    std::error_code ec;
    if (auto parentDir = location.parent_path(); !parentDir.empty()) {
        fs::create_directories(parentDir, ec);
        ec.clear();
    }
    std::ofstream out(location, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(out);
}

bool DesktopStorage::readBytes(const Path& location,
                               std::vector<unsigned char>& out) const {
    return platform::readFileBytes(location, out);
}

bool DesktopStorage::rename(const Path& from,
                            const std::string& newName) {
    if (newName.empty()) {
        return false;
    }
    std::error_code ec;
    fs::rename(from, from.parent_path() / platform::pathFromUtf8(newName), ec);
    return !ec;
}

bool DesktopStorage::remove(const Path& location) {
    std::error_code ec;
    return fs::remove(location, ec) && !ec;
}

Storage::Path DesktopStorage::join(const Path& base,
                                   const Path& child) const {
    return base / child;
}

Storage::Path DesktopStorage::parent(const Path& location) const {
    return location.parent_path();
}

bool DesktopStorage::scanRecursive(const Path& root,
                                   std::vector<Entry>& out) const {
    out.clear();
    std::error_code ec;
    if (root.empty() || !fs::is_directory(root, ec) || ec) {
        return false;
    }

    for (auto it = fs::recursive_directory_iterator(
             root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }

        const fs::path& path = it->path();
        bool isDir = it->is_directory(ec) && !ec;
        ec.clear();
        if (!isDir && !it->is_regular_file(ec)) {
            ec.clear();
            continue;
        }
        ec.clear();

        const fs::path relative = fs::relative(path, root, ec);
        if (ec) {
            ec.clear();
            continue;
        }
        out.push_back({relative, isDir});
    }
    return true;
}

bool DesktopStorage::scanFlat(const Path& folder,
                              std::vector<Item>& out) const {
    out.clear();
    std::error_code ec;
    if (folder.empty() || !fs::is_directory(folder, ec) || ec) {
        return false;
    }

    for (const auto& entry : fs::directory_iterator(
             folder, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!entry.is_regular_file(ec) || ec) {
            ec.clear();
            continue;
        }
        out.push_back({entry.path(),
                       platform::pathToUtf8(entry.path().filename())});
    }
    return true;
}

}  // namespace ab
