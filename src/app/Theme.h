#pragma once

#include <string>

struct ImFont;
struct ImGuiIO;

namespace ab {

// Applies a color theme by name. Recognized names:
//   "catppuccin-mocha", "rose-pine", "gruvbox",
//   "dark", "classic", "light".
void applyTheme(const std::string& name);

// Fonts used by the application (and markdown renderer).
struct FontSet {
    ImFont* regular = nullptr;  // body text
    ImFont* bold = nullptr;      // strong / table headers
    ImFont* h1 = nullptr;        // level-1 headings (larger bold)
};

// Loads larger, sharper system fonts if available; falls back to the
// default ImGui font (scaled up). Call once after ImGui::CreateContext().
void loadFonts(ImGuiIO& io);

// Returns the loaded font set (valid after loadFonts()).
const FontSet& fonts();

}  // namespace ab