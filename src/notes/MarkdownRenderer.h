#pragma once

#include <filesystem>
#include <string>

namespace ab {

// Renders Markdown text (UTF-8) using Dear ImGui, via the MD4C parser.
class MarkdownRenderer {
public:
    // Renders `markdown` into the current ImGui context (call between
    // ImGui::Begin/End). Relative image URLs are resolved against `baseDir`.
    static void render(const std::string& markdown,
                       const std::filesystem::path& baseDir = {});
};

}  // namespace ab