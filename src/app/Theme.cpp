#include "app/Theme.h"

#include <imgui.h>

#include <cstdio>
#include <filesystem>

namespace ab {

namespace fs = std::filesystem;

void applyTheme(const std::string& name) {
    if (name == "light") {
        ImGui::StyleColorsLight();
    } else if (name == "classic") {
        ImGui::StyleColorsClassic();
    } else {
        ImGui::StyleColorsDark();
    }

    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 6.0f;
    s.ChildRounding = 6.0f;
    s.FrameRounding = 4.0f;
    s.GrabRounding = 4.0f;
    s.PopupRounding = 6.0f;
    s.ScrollbarRounding = 9.0f;
    s.TabRounding = 4.0f;
    s.FramePadding = ImVec2(6.0f, 4.0f);
    s.ItemSpacing = ImVec2(8.0f, 6.0f);
    s.WindowPadding = ImVec2(10.0f, 10.0f);
}

void loadFonts(ImGuiIO& io) {
    // Allow an explicit override via environment variable.
    const char* override = std::getenv("AB_FONT");
    if (override && *override) {
        io.Fonts->AddFontFromFileTTF(override, 18.0f);
        return;
    }

    static const char* kCandidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/run/current-system/sw/share/X11/fonts/dejavu/DejaVuSans.ttf",
    };
    for (const char* path : kCandidates) {
        if (fs::exists(path)) {
            io.Fonts->AddFontFromFileTTF(path, 18.0f);
            return;
        }
    }

    // Fall back to the embedded font, scaled up for readability.
    io.Fonts->AddFontDefault();
    io.FontGlobalScale = 1.4f;
}

}  // namespace ab