#pragma once

#include <string>

namespace ab {

// Renders Markdown text (UTF-8) using Dear ImGui, via the MD4C parser.
class MarkdownRenderer {
public:
    // Renders `markdown` into the current ImGui context (call between
    // ImGui::Begin/End).
    static void render(const std::string& markdown);
};

}  // namespace ab