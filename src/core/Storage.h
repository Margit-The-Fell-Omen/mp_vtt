#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ab {

// Backend-neutral file access. Locations are opaque: filesystem paths on
// desktop, SAF document/tree URIs on Android. Services only see this interface.
class Storage {
public:
    using Path = std::filesystem::path;

    struct Entry {
        Path path;          // path relative to the scanned root
        bool isDir = false;
    };

    struct Item {
        Path location;      // absolute/opaque location
        std::string name;   // display name
    };

    virtual ~Storage() = default;

    virtual bool exists(const Path& location) const = 0;
    virtual bool isDirectory(const Path& location) const = 0;
    virtual bool isRegularFile(const Path& location) const = 0;

    virtual bool readText(const Path& location, std::string& out) const = 0;
    virtual bool writeText(const Path& location,
                           const std::string& content) = 0;
    virtual bool readBytes(const Path& location,
                           std::vector<unsigned char>& out) const = 0;

    virtual bool rename(const Path& from,
                        const std::string& newName) = 0;
    virtual bool remove(const Path& location) = 0;

    // Appends a relative child to a base location.
    virtual Path join(const Path& base, const Path& child) const = 0;
    virtual Path parent(const Path& location) const = 0;

    // Recursively lists a note root. Entry paths are relative to the root.
    virtual bool scanRecursive(const Path& root,
                               std::vector<Entry>& out) const = 0;

    // Lists the regular files directly inside a folder.
    virtual bool scanFlat(const Path& folder, std::vector<Item>& out) const = 0;
};

}  // namespace ab
