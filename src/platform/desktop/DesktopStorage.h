#pragma once

#include "core/Storage.h"

namespace ab {

// Local filesystem backend used on Linux and Windows.
class DesktopStorage final : public Storage {
public:
    bool exists(const Path& location) const override;
    bool isDirectory(const Path& location) const override;
    bool isRegularFile(const Path& location) const override;

    bool readText(const Path& location, std::string& out) const override;
    bool writeText(const Path& location,
                   const std::string& content) override;
    bool readBytes(const Path& location,
                   std::vector<unsigned char>& out) const override;

    bool rename(const Path& from,
                const std::string& newName) override;
    bool remove(const Path& location) override;

    Path join(const Path& base, const Path& child) const override;
    Path parent(const Path& location) const override;

    bool scanRecursive(const Path& root,
                       std::vector<Entry>& out) const override;
    bool scanFlat(const Path& folder, std::vector<Item>& out) const override;
};

}  // namespace ab
