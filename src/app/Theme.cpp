#include "app/Theme.h"

#include <imgui.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#ifdef AB_HAVE_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

namespace ab {

namespace fs = std::filesystem;

namespace {

// Hex color -> ImVec4 (full alpha).
ImVec4 hex(int rr, int gg, int bb, int aa = 255) {
    return ImVec4(rr / 255.0f, gg / 255.0f, bb / 255.0f, aa / 255.0f);
}

struct Palette {
    const char* name;
    ImVec4 base;            // window/child bg
    ImVec4 baseText;        // default text
    ImVec4 mutedText;       // disabled text
    ImVec4 header;          // header / title bg
    ImVec4 headerText;      // header text
    ImVec4 hovered;         // hovered group
    ImVec4 active;          // active group
    ImVec4 frameBg;         // frame/input bg
    ImVec4 frameHovered;
    ImVec4 frameActive;
    ImVec4 titleBg;         // title bar bg
    ImVec4 titleBgActive;
    ImVec4 tab;             // tab bg
    ImVec4 tabActive;       // active tab bg
    ImVec4 button;          // button bg
    ImVec4 buttonHovered;
    ImVec4 buttonActive;
    ImVec4 border;
    ImVec4 accent;          // sliders/checks/selection accent
    ImVec4 accentText;      // checkmarks / selection text accent
    ImVec4 accentBg;        // selection bg
    ImVec4 separator;
};

void applyPalette(const Palette& p) {
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;

    c[ImGuiCol_Text] = p.baseText;
    c[ImGuiCol_TextDisabled] = p.mutedText;
    c[ImGuiCol_WindowBg] = p.base;
    c[ImGuiCol_ChildBg] = p.base;
    c[ImGuiCol_PopupBg] = p.frameBg;
    c[ImGuiCol_Border] = p.border;
    c[ImGuiCol_BorderShadow] = hex(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = p.frameBg;
    c[ImGuiCol_FrameBgHovered] = p.frameHovered;
    c[ImGuiCol_FrameBgActive] = p.frameActive;
    c[ImGuiCol_TitleBg] = p.titleBg;
    c[ImGuiCol_TitleBgActive] = p.titleBgActive;
    c[ImGuiCol_TitleBgCollapsed] = p.titleBg;
    c[ImGuiCol_MenuBarBg] = p.header;
    c[ImGuiCol_ScrollbarBg] = p.frameBg;
    c[ImGuiCol_ScrollbarGrab] = p.button;
    c[ImGuiCol_ScrollbarGrabHovered] = p.buttonHovered;
    c[ImGuiCol_ScrollbarGrabActive] = p.buttonActive;
    c[ImGuiCol_CheckMark] = p.accentText;
    c[ImGuiCol_SliderGrab] = p.accent;
    c[ImGuiCol_SliderGrabActive] = p.accent;
    c[ImGuiCol_Button] = p.button;
    c[ImGuiCol_ButtonHovered] = p.buttonHovered;
    c[ImGuiCol_ButtonActive] = p.buttonActive;
    c[ImGuiCol_Header] = p.header;
    c[ImGuiCol_HeaderHovered] = p.hovered;
    c[ImGuiCol_HeaderActive] = p.active;
    c[ImGuiCol_Separator] = p.separator;
    c[ImGuiCol_SeparatorHovered] = p.active;
    c[ImGuiCol_SeparatorActive] = p.accent;
    c[ImGuiCol_ResizeGrip] = p.button;
    c[ImGuiCol_ResizeGripHovered] = p.buttonHovered;
    c[ImGuiCol_ResizeGripActive] = p.accent;
    c[ImGuiCol_Tab] = p.tab;
    c[ImGuiCol_TabHovered] = p.hovered;
    c[ImGuiCol_TabSelected] = p.tabActive;
    c[ImGuiCol_TabDimmed] = p.tab;
    c[ImGuiCol_TabDimmedSelected] = p.tabActive;
    c[ImGuiCol_TabSelectedOverline] = p.accent;
    c[ImGuiCol_TabDimmedSelectedOverline] = p.accent;
    c[ImGuiCol_TableHeaderBg] = p.header;
    c[ImGuiCol_TableBorderStrong] = p.border;
    c[ImGuiCol_TableBorderLight] = p.border;
    c[ImGuiCol_TableRowBg] = hex(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = p.frameHovered;

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

}  // namespace

void applyTheme(const std::string& name) {
    if (name == "light") {
        ImGui::StyleColorsLight();
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
        return;
    }

    if (name == "classic") {
        // "classic" is the darker of the two built-in dark themes.
        ImGui::StyleColorsClassic();
        ImGui::GetStyle().FramePadding = ImVec2(6.0f, 4.0f);
        ImGui::GetStyle().ItemSpacing = ImVec2(8.0f, 6.0f);
        return;
    }

    Palette p{};

    if (name == "catppuccin-mocha") {
        p.name = "catppuccin-mocha";
        p.base = hex(30, 30, 46);        // base
        p.baseText = hex(205, 214, 244); // text
        p.mutedText = hex(127, 132, 156); // overlay1
        p.header = hex(69, 71, 90);      // surface0
        p.headerText = hex(205, 214, 244);
        p.hovered = hex(88, 91, 112);    // surface1
        p.active = hex(108, 112, 134);   // surface2
        p.frameBg = hex(49, 50, 68);     // surface0
        p.frameHovered = hex(88, 91, 112);
        p.frameActive = hex(108, 112, 134);
        p.titleBg = hex(24, 24, 37);     // mantle
        p.titleBgActive = hex(69, 71, 90);
        p.tab = hex(36, 39, 58);         // mantle-ish
        p.tabActive = hex(49, 50, 68);
        p.button = hex(69, 71, 90);
        p.buttonHovered = hex(88, 91, 112);
        p.buttonActive = hex(108, 112, 134);
        p.border = hex(69, 71, 90);
        p.accent = hex(137, 180, 250);   // blue
        p.accentText = hex(166, 227, 161); // green
        p.accentBg = hex(89, 99, 168);
        p.separator = hex(69, 71, 90);
    } else if (name == "rose-pine") {
        p.name = "rose-pine";
        p.base = hex(25, 23, 36);        // base
        p.baseText = hex(224, 222, 244); // text
        p.mutedText = hex(111, 106, 133); // muted
        p.header = hex(38, 35, 58);      // surface
        p.headerText = hex(224, 222, 244);
        p.hovered = hex(47, 45, 80);     // overlay
        p.active = hex(85, 79, 122);     // highlightMed
        p.frameBg = hex(38, 35, 58);
        p.frameHovered = hex(47, 45, 80);
        p.frameActive = hex(85, 79, 122);
        p.titleBg = hex(25, 23, 36);
        p.titleBgActive = hex(38, 35, 58);
        p.tab = hex(31, 29, 46);
        p.tabActive = hex(38, 35, 58);
        p.button = hex(38, 35, 58);
        p.buttonHovered = hex(47, 45, 80);
        p.buttonActive = hex(85, 79, 122);
        p.border = hex(38, 35, 58);
        p.accent = hex(235, 111, 146);   // love
        p.accentText = hex(156, 207, 216); // foam
        p.accentBg = hex(85, 79, 122);
        p.separator = hex(38, 35, 58);
    } else if (name == "gruvbox") {
        p.name = "gruvbox";
        p.base = hex(40, 40, 40);        // bg0
        p.baseText = hex(235, 219, 178); // fg
        p.mutedText = hex(146, 131, 116); // gray
        p.header = hex(60, 56, 54);      // bg2
        p.headerText = hex(235, 219, 178);
        p.hovered = hex(80, 73, 69);     // bg3
        p.active = hex(102, 92, 84);     // bg4
        p.frameBg = hex(50, 48, 47);     // bg1
        p.frameHovered = hex(80, 73, 69);
        p.frameActive = hex(102, 92, 84);
        p.titleBg = hex(40, 40, 40);
        p.titleBgActive = hex(60, 56, 54);
        p.tab = hex(29, 32, 33);         // bg0_h
        p.tabActive = hex(60, 56, 54);
        p.button = hex(60, 56, 54);
        p.buttonHovered = hex(80, 73, 69);
        p.buttonActive = hex(102, 92, 84);
        p.border = hex(60, 56, 54);
        p.accent = hex(215, 153, 33);    // yellow
        p.accentText = hex(184, 187, 38); // green
        p.accentBg = hex(102, 92, 84);
        p.separator = hex(60, 56, 54);
    } else {
        // "dark" (default ImGui dark, slightly lighter than classic).
        ImGui::StyleColorsDark();
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
        return;
    }

    applyPalette(p);
}

// Builds a glyph range covering the default ranges plus the round bullet
// (U+2022) so unordered-list markers render correctly.
static const ImWchar* bulletGlyphRanges() {
    static ImVector<ImWchar> ranges;
    if (ranges.empty()) {
        ImFontGlyphRangesBuilder builder;
        // Same base range ImGui uses by default (ASCII + Latin-1) plus
        // typographic punctuation markers.
        builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesDefault());
        builder.AddChar(0x2022);  // •
        builder.AddChar(0x2013);  // – en dash
        builder.AddChar(0x2014);  // — em dash
        builder.AddChar(0x2026);  // …
        builder.BuildRanges(&ranges);
    }
    return ranges.Data;
}

#ifdef AB_HAVE_FONTCONFIG
// Resolves the on-disk path of a preferred font via fontconfig.
static std::string findFontPath(const char* family) {
    FcPattern* pat = FcPatternCreate();
    FcPatternAddString(pat, FC_FAMILY, reinterpret_cast<const FcChar8*>(family));
    FcPatternAddBool(pat, FC_SCALABLE, FcTrue);
    FcConfigSubstitute(nullptr, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    std::string result;
    FcResult res = FcResultNoMatch;
    FcPattern* match = FcFontMatch(nullptr, pat, &res);
    if (match) {
        FcChar8* file = nullptr;
        if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch && file) {
            result = reinterpret_cast<const char*>(file);
        }
        FcPatternDestroy(match);
    }
    FcPatternDestroy(pat);
    return result;
}
#endif

void loadFonts(ImGuiIO& io) {
    const char* override = std::getenv("AB_FONT");
    if (override && *override) {
        io.Fonts->AddFontFromFileTTF(override, 18.0f, nullptr,
                                     bulletGlyphRanges());
        return;
    }

#ifdef AB_HAVE_FONTCONFIG
    const char* families[] = {"DejaVu Sans", "Liberation Sans",
                              "Noto Sans", "Cantarell"};
    for (const char* family : families) {
        std::string path = findFontPath(family);
        if (!path.empty()) {
            if (auto* f = io.Fonts->AddFontFromFileTTF(path.c_str(), 18.0f, nullptr,
                                                       bulletGlyphRanges())) {
                return;
            }
        }
    }
#endif

    // Hardcoded fallbacks (non-Nix systems).
    static const char* kCandidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    };
    for (const char* path : kCandidates) {
        if (fs::exists(path)) {
            if (io.Fonts->AddFontFromFileTTF(path, 18.0f, nullptr,
                                             bulletGlyphRanges())) {
                return;
            }
        }
    }

    io.Fonts->AddFontDefault();
    io.FontGlobalScale = 1.4f;
}

}  // namespace ab