#pragma once

#include <string>

namespace ab {

// Represents the current project: a root folder containing notes.
class Project {
public:
    // Returns true if a root folder is currently set and it exists.
    bool isValid() const { return !m_rootPath.empty(); }

    const std::string& rootPath() const { return m_rootPath; }
    void setRootPath(const std::string& path) { m_rootPath = path; }

private:
    std::string m_rootPath;
};

}  // namespace ab