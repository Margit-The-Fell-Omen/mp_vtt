#pragma once

#include <string>

struct ImGuiIO;

namespace ab {

// Applies a color theme ("dark", "light" or "classic") to ImGui.
void applyTheme(const std::string& name);

// Loads a larger, sharper system font if available; falls back to the
// default ImGui font (scaled up). Call once after ImGui::CreateContext().
void loadFonts(ImGuiIO& io);

}  // namespace ab