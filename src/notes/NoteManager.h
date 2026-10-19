#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace ab {

namespace fs = std::filesystem;

// Manages the note folder tree and individual .md note content.
class NoteManager {
public:
    using Path = fs::path;

    // Scans the root (destructively) and rebuilds the folder/file tree.
    void rescan(const Path& root);

    const Path& root() const { return m_root; }
    bool hasRoot() const { return !m_root.empty(); }

    // Directories, sorted (files-after-folders). Each entry is a relative path.
    struct Entry {
        Path path;        // relative to root
        bool isDir = false;
    };

    const std::vector<Entry>& entries(const Path& dirRelative) const;

    // Reads a note's contents. Returns false if unreadable/non-existent.
    bool read(const Path& relativePath, std::string& out) const;

    // Writes a note's contents (creates parent dirs if needed).
    bool write(const Path& relativePath, const std::string& content);

    // Creates an empty note at relativePath.
    bool create(const Path& relativePath);

    // Renames (or moves) a note/dir. Returns false on failure or collision.
    bool rename(const Path& from, const Path& to);

    // Deletes a note (file only). Directories with children are rejected.
    bool remove(const Path& relativePath);

    // Absolute filesystem path for a relative path.
    Path absolute(const Path& relativePath) const { return m_root / relativePath; }

private:
    bool isMarkdown(const Path& p) const;

    Path m_root;
    std::vector<Entry> m_entries;  // flat list for the tree (recursive walk)
};

}  // namespace ab