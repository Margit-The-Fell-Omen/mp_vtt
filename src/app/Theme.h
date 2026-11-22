#pragma once

#include <string>
#include <vector>

struct ImFont;
struct ImGuiIO;
struct ImVec4;

namespace ab {

// A selectable theme. Every theme is either light or dark.
struct ThemeInfo {
    const char* id = nullptr;      // stable id, persisted in settings
    const char* label = nullptr;   // display name
    bool isLight = false;          // theme family (light or dark)
};

// Returns all themes (dark group first, then light group). Drives both the
// theme selector and next/previous cycling.
const std::vector<ThemeInfo>& availableThemes();

// Returns true when the currently applied theme is a light theme.
bool isLightTheme();

// Accent color of the currently applied theme (sliders, checks, selection).
ImVec4 accentColor();

// Warning/highlight color tuned for the current light/dark mode.
ImVec4 warningColor();

// Applies a color theme by id. Unknown ids fall back to ImGui Dark.
void applyTheme(const std::string& name);

// Fonts used by the application (and markdown renderer).
struct FontSet {
    ImFont* regular = nullptr;    // body text
    ImFont* bold = nullptr;       // strong / table headers
    ImFont* italic = nullptr;     // emphasis
    ImFont* boldItalic = nullptr; // strong + emphasis
    ImFont* mono = nullptr;       // inline code / fenced code blocks
    ImFont* h1 = nullptr;         // level-1 headings (larger bold)
};

// Loads the embedded Liberation fonts; falls back to the default ImGui
// font (scaled up) only if the embedded data is missing. Call once after
// ImGui::CreateContext().
void loadFonts(ImGuiIO& io);

// Returns the loaded font set (valid after loadFonts()).
const FontSet& fonts();

}  // namespace ab