#pragma once

#include <string>

struct ImGuiIO;

namespace ab {

// Applies a color theme by name. Recognized names:
//   "catppuccin-mocha", "rose-pine", "gruvbox",
//   "dark", "classic", "light".
void applyTheme(const std::string& name);

// Loads a larger, sharper system font if available; falls back to the
// default ImGui font (scaled up). Call once after ImGui::CreateContext().
void loadFonts(ImGuiIO& io);

}  // namespace ab