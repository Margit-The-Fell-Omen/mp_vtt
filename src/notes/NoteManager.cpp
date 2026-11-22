#include "notes/NoteManager.h"

#include "platform/Platform.h"

#include <algorithm>
#include <cctype>

namespace ab {

void NoteManager::rescan(const Path& root) {
    m_root = root;
    m_entries.clear();

    if (root.empty() || !m_storage) {
        return;
    }

    std::vector<Storage::Entry> found;
    if (!m_storage->scanRecursive(root, found)) {
        return;
    }

    std::vector<Entry> entries;
    entries.reserve(found.size());
    for (const auto& e : found) {
        if (!e.isDir && !isMarkdown(e.path)) {
            continue;
        }
        entries.push_back({e.path, e.isDir});
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
    if (!m_storage) {
        return false;
    }
    return m_storage->readText(absolute(relativePath), out);
}

bool NoteManager::write(const Path& relativePath, const std::string& content) {
    if (!m_storage) {
        return false;
    }
    return m_storage->writeText(absolute(relativePath), content);
}

bool NoteManager::create(const Path& relativePath) {
    if (!m_storage || !isMarkdown(relativePath)) {
        return false;
    }
    Path p = absolute(relativePath);
    if (m_storage->exists(p)) {
        return false;
    }
    return m_storage->writeText(p, "");
}

bool NoteManager::rename(const Path& from, const Path& to) {
    if (!m_storage) {
        return false;
    }
    Path src = absolute(from);
    Path dst = absolute(to);
    if (!m_storage->exists(src) || m_storage->exists(dst)) {
        return false;
    }
    return m_storage->rename(src, platform::pathToUtf8(to.filename()));
}

bool NoteManager::remove(const Path& relativePath) {
    if (!m_storage) {
        return false;
    }
    Path p = absolute(relativePath);
    if (!m_storage->isRegularFile(p)) {
        return false;
    }
    return m_storage->remove(p);
}

bool NoteManager::isMarkdown(const Path& p) const {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return ext == ".md";
}

}  // namespace ab