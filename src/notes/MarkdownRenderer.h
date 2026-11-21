#pragma once

#include <filesystem>
#include <string>

namespace ab {

class Storage;

// Renders Markdown text (UTF-8) using Dear ImGui, via the MD4C parser.
class MarkdownRenderer {
public:
    // Renders `markdown` into the current ImGui context (call between
    // ImGui::Begin/End). Relative image URLs are resolved against `baseDir`
    // through `storage`.
    static void render(const std::string& markdown,
                       const std::filesystem::path& baseDir,
                       Storage& storage);
};

}  // namespace ab