#include "notes/NoteManager.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace ab {

void NoteManager::rescan(const Path& root) {
    m_root = root;
    m_entries.clear();

    if (root.empty() || !fs::exists(root) || !fs::is_directory(root)) {
        return;
    }

    // Recursive walk, collecting dirs and markdown files.
    std::vector<Entry> entries;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(
             root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }

        const auto& p = it->path();
        Entry e;
        if (it->is_directory(ec) && !ec) {
            e.isDir = true;
        } else {
            if (!isMarkdown(p)) {
                continue;
            }
            e.isDir = false;
        }
        ec.clear();
        e.path = fs::relative(p, root);
        entries.push_back(std::move(e));
    }

    // Sort: directories first (alphabetically), then files.
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.isDir != b.isDir) {
            return a.isDir;  // dirs first
        }
        return a.path < b.path;
    });

    m_entries = std::move(entries);
}

const std::vector<NoteManager::Entry>& NoteManager::entries(
    const Path& dirRelative) const {
    (void)dirRelative;  // flat list already covers the whole tree
    return m_entries;
}

bool NoteManager::read(const Path& relativePath, std::string& out) const {
    Path p = absolute(relativePath);
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool NoteManager::write(const Path& relativePath, const std::string& content) {
    Path p = absolute(relativePath);
    std::error_code ec;
    if (auto parent = p.parent_path(); !parent.empty()) {
        fs::create_directories(parent, ec);
    }
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(out);
}

bool NoteManager::create(const Path& relativePath) {
    if (!isMarkdown(relativePath)) {
        return false;
    }
    Path p = absolute(relativePath);
    if (fs::exists(p)) {
        return false;
    }
    return write(relativePath, "");
}

bool NoteManager::rename(const Path& from, const Path& to) {
    Path src = absolute(from);
    Path dst = absolute(to);
    if (!fs::exists(src) || fs::exists(dst)) {
        return false;
    }
    std::error_code ec;
    fs::rename(src, dst, ec);
    return !ec;
}

bool NoteManager::remove(const Path& relativePath) {
    Path p = absolute(relativePath);
    std::error_code ec;
    if (!fs::is_regular_file(p, ec) || ec) {
        return false;
    }
    return fs::remove(p, ec) && !ec;
}

bool NoteManager::isMarkdown(const Path& p) const {
    return p.extension() == ".md";
}

}  // namespace ab