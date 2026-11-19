#include "app/Theme.h"

#include <imgui.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

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

// Blends a toward b. Used to derive surface colors from seed colors so a
// single base/accent pair works for both light and dark themes.
ImVec4 mix(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t, 1.0f);
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

// Small set of hand-picked colors; every other UI shade is derived from these
// by makePalette(). One entry per theme keeps the registry readable.
struct PaletteSeed {
    ImVec4 base;        // window/child bg
    ImVec4 text;        // default text
    ImVec4 muted;       // disabled text
    ImVec4 accent;      // sliders/checks/selection accent
    ImVec4 accentText;  // checkmarks / selection text accent
};

Palette makePalette(const char* id, const PaletteSeed& s) {
    Palette p;
    p.name = id;
    const ImVec4& base = s.base;
    const ImVec4& text = s.text;
    p.base = base;
    p.baseText = text;
    p.mutedText = s.muted;
    p.header = mix(base, text, 0.10f);
    p.headerText = text;
    p.hovered = mix(base, text, 0.16f);
    p.active = mix(base, text, 0.24f);
    p.frameBg = mix(base, text, 0.06f);
    p.frameHovered = mix(base, text, 0.13f);
    p.frameActive = mix(base, text, 0.20f);
    p.titleBg = base;
    p.titleBgActive = p.header;
    p.tab = mix(base, text, 0.04f);
    p.tabActive = p.header;
    p.button = p.header;
    p.buttonHovered = p.hovered;
    p.buttonActive = p.active;
    p.border = mix(base, text, 0.14f);
    p.accent = s.accent;
    p.accentText = s.accentText;
    p.accentBg = mix(s.accent, base, 0.45f);
    p.separator = p.border;
    return p;
}

// Explicit palettes for the original themes (kept as-is so their current
// look is preserved).
Palette catppuccinMocha() {
    Palette p;
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
    return p;
}

Palette rosePine() {
    Palette p;
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
    return p;
}

Palette gruvbox() {
    Palette p;
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
    return p;
}

struct ThemeDef {
    const char* id;
    const char* label;
    bool isLight;
    Palette palette;
};

// All curated palettes. Existing ids (catppuccin-mocha, rose-pine, gruvbox)
// are kept for backward compatibility.
static const ThemeDef kPaletteThemes[] = {
    {"catppuccin-mocha", "Catppuccin Mocha", false, catppuccinMocha()},
    {"catppuccin-latte", "Catppuccin Latte", true,
     makePalette("catppuccin-latte",
                 {hex(239, 241, 245), hex(76, 79, 105), hex(108, 111, 133),
                  hex(30, 102, 245), hex(64, 160, 43)})},
    {"rose-pine", "Rosé Pine", false, rosePine()},
    {"rose-pine-dawn", "Rosé Pine Dawn", true,
     makePalette("rose-pine-dawn",
                 {hex(250, 244, 237), hex(87, 82, 121), hex(155, 148, 184),
                  hex(180, 99, 122), hex(116, 166, 165)})},
    {"gruvbox", "Gruvbox Dark", false, gruvbox()},
    {"gruvbox-light", "Gruvbox Light", true,
     makePalette("gruvbox-light",
                 {hex(251, 241, 199), hex(60, 56, 54), hex(146, 131, 116),
                  hex(215, 153, 33), hex(66, 123, 88)})},
    {"dracula", "Dracula", false,
     makePalette("dracula",
                 {hex(40, 42, 54), hex(248, 248, 242), hex(98, 114, 164),
                  hex(189, 147, 249), hex(80, 250, 123)})},
    {"alucard", "Alucard (Dracula Light)", true,
     makePalette("alucard",
                 {hex(248, 248, 242), hex(40, 42, 54), hex(98, 114, 164),
                  hex(189, 147, 249), hex(80, 250, 123)})},
    {"nord", "Nord", false,
     makePalette("nord",
                 {hex(46, 52, 64), hex(216, 222, 233), hex(76, 86, 106),
                  hex(136, 192, 208), hex(143, 188, 187)})},
    {"tokyo-night", "Tokyo Night", false,
     makePalette("tokyo-night",
                 {hex(26, 27, 38), hex(192, 202, 245), hex(86, 95, 137),
                  hex(122, 162, 247), hex(158, 206, 106)})},
    {"tokyo-night-light", "Tokyo Night Light", true,
     makePalette("tokyo-night-light",
                 {hex(233, 233, 237), hex(58, 64, 94), hex(110, 111, 143),
                  hex(46, 125, 233), hex(26, 127, 55)})},
    {"one-dark", "One Dark", false,
     makePalette("one-dark",
                 {hex(40, 44, 52), hex(171, 178, 191), hex(92, 99, 112),
                  hex(97, 175, 239), hex(152, 195, 121)})},
    {"one-light", "One Light", true,
     makePalette("one-light",
                 {hex(250, 250, 250), hex(56, 58, 66), hex(142, 144, 158),
                  hex(64, 120, 242), hex(80, 161, 79)})},
    {"solarized-dark", "Solarized Dark", false,
     makePalette("solarized-dark",
                 {hex(0, 43, 54), hex(147, 161, 161), hex(88, 110, 117),
                  hex(38, 139, 210), hex(42, 161, 152)})},
    {"solarized-light", "Solarized Light", true,
     makePalette("solarized-light",
                 {hex(253, 246, 227), hex(88, 110, 117), hex(147, 161, 161),
                  hex(38, 139, 210), hex(42, 161, 152)})},
    {"monokai", "Monokai", false,
     makePalette("monokai",
                 {hex(39, 40, 34), hex(248, 248, 242), hex(117, 113, 94),
                  hex(249, 38, 114), hex(166, 226, 46)})},
    {"everforest-dark", "Everforest Dark", false,
     makePalette("everforest-dark",
                 {hex(45, 53, 59), hex(211, 198, 170), hex(133, 146, 137),
                  hex(167, 192, 128), hex(219, 188, 127)})},
    {"everforest-light", "Everforest Light", true,
     makePalette("everforest-light",
                 {hex(253, 246, 227), hex(92, 106, 112), hex(157, 169, 160),
                  hex(131, 164, 117), hex(219, 188, 127)})},
    {"kanagawa-wave", "Kanagawa Wave", false,
     makePalette("kanagawa-wave",
                 {hex(31, 31, 40), hex(220, 215, 186), hex(114, 113, 105),
                  hex(126, 156, 216), hex(152, 187, 108)})},
    {"kanagawa-lotus", "Kanagawa Lotus", true,
     makePalette("kanagawa-lotus",
                 {hex(250, 244, 236), hex(84, 84, 100), hex(140, 140, 167),
                  hex(52, 137, 200), hex(152, 187, 108)})},
};

bool g_isLightTheme = false;

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
    c[ImGuiCol_TextSelectedBg] = p.accentBg;
    c[ImGuiCol_InputTextCursor] = p.baseText;
    c[ImGuiCol_NavCursor] = p.accent;
    c[ImGuiCol_DragDropTarget] = p.accentText;

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

bool isLightTheme() {
    return g_isLightTheme;
}

ImVec4 accentColor() {
    return ImGui::GetStyle().Colors[ImGuiCol_SliderGrab];
}

ImVec4 warningColor() {
    // Mode-aware warning tint (unsaved-changes marker etc.).
    return g_isLightTheme ? hex(160, 82, 0) : hex(255, 179, 77);
}

const std::vector<ThemeInfo>& availableThemes() {
    static const std::vector<ThemeInfo> all = [] {
        std::vector<ThemeInfo> v;
        for (const auto& t : kPaletteThemes) {
            if (!t.isLight) {
                v.push_back({t.id, t.label, false});
            }
        }
        v.push_back({"dark", "ImGui Dark", false});
        v.push_back({"classic", "ImGui Classic", false});
        for (const auto& t : kPaletteThemes) {
            if (t.isLight) {
                v.push_back({t.id, t.label, true});
            }
        }
        v.push_back({"light", "ImGui Light", true});
        return v;
    }();
    return all;
}

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
        g_isLightTheme = true;
        return;
    }

    if (name == "classic") {
        // "classic" is the darker of the two built-in dark themes.
        ImGui::StyleColorsClassic();
        ImGui::GetStyle().FramePadding = ImVec2(6.0f, 4.0f);
        ImGui::GetStyle().ItemSpacing = ImVec2(8.0f, 6.0f);
        g_isLightTheme = false;
        return;
    }

    for (const auto& t : kPaletteThemes) {
        if (name == t.id) {
            applyPalette(t.palette);
            g_isLightTheme = t.isLight;
            return;
        }
    }

    // "dark" (default ImGui dark) and any unknown id.
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
    g_isLightTheme = false;
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
static std::string findFontPath(const char* family, const char* style = nullptr) {
    FcPattern* pat = FcPatternCreate();
    FcPatternAddString(pat, FC_FAMILY, reinterpret_cast<const FcChar8*>(family));
    if (style && *style) {
        FcPatternAddString(pat, FC_STYLE, reinterpret_cast<const FcChar8*>(style));
    }
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

namespace {

FontSet g_fonts;

}  // namespace

const FontSet& fonts() {
    return g_fonts;
}

// Resolves a bold variant of a given font path (same directory), else empty.
// Finds a sibling font file (same directory) using common naming conventions.
static std::string siblingFontPath(const std::string& regularPath,
                                   std::initializer_list<const char*> names) {
    if (regularPath.empty()) {
        return "";
    }
    fs::path p(regularPath);
    std::string stem = p.stem().string();   // e.g. DejaVuSans
    std::string parent = p.parent_path().string();
    std::string ext = p.extension().string();
    for (const char* suffix : names) {
        fs::path cand = fs::path(parent) / (stem + "-" + suffix + ext);
        if (fs::exists(cand)) {
            return cand.string();
        }
        cand = fs::path(parent) / (stem + suffix + ext);
        if (fs::exists(cand)) {
            return cand.string();
        }
    }
    return "";
}

static ImFont* addFont(ImGuiIO& io, const std::string& path, float size) {
    if (path.empty()) {
        return nullptr;
    }
    return io.Fonts->AddFontFromFileTTF(path.c_str(), size, nullptr,
                                        bulletGlyphRanges());
}

void loadFonts(ImGuiIO& io) {
    const char* override = std::getenv("AB_FONT");
    if (override && *override) {
        ImFont* f = addFont(io, override, 18.0f);
        g_fonts.regular = f;
        g_fonts.bold = f;
        g_fonts.italic = f;
        g_fonts.boldItalic = f;
        g_fonts.h1 = f;
        return;
    }

    // 1. Preferred family via fontconfig.
    std::string regularPath;
    std::string boldPath;
    std::string italicPath;
    std::string boldItalicPath;
#ifdef AB_HAVE_FONTCONFIG
    const char* families[] = {"DejaVu Sans", "Liberation Sans",
                              "Noto Sans", "Cantarell"};
    for (const char* family : families) {
        regularPath = findFontPath(family);
        if (!regularPath.empty()) {
            auto good = [&](const std::string& p, const std::string& base) {
                return !p.empty() && p != base;
            };
            // Bold-italic must be distinct from regular, bold AND italic,
            // otherwise ***text*** would render as plain italic-only.
            auto goodBI = [&](const std::string& p) {
                return good(p, regularPath) && good(p, boldPath) &&
                       (italicPath.empty() || good(p, italicPath));
            };
            boldPath = findFontPath(family, "Bold");
            if (!good(boldPath, regularPath)) {
                boldPath = siblingFontPath(regularPath, {"Bold", "b", "B"});
            }
            italicPath = findFontPath(family, "Italic");
            if (!good(italicPath, regularPath)) {
                italicPath = findFontPath(family, "Oblique");
            }
            if (!good(italicPath, regularPath)) {
                italicPath =
                    siblingFontPath(regularPath, {"Oblique", "Italic", "Obl"});
            }
            boldItalicPath = findFontPath(family, "Bold Italic");
            if (!goodBI(boldItalicPath)) {
                boldItalicPath = findFontPath(family, "Bold Oblique");
            }
            if (!goodBI(boldItalicPath)) {
                boldItalicPath = siblingFontPath(
                    regularPath, {"BoldOblique", "BoldItalic"});
            }
            break;
        }
    }
#endif

    // 2. Hardcoded fallbacks (non-Nix systems).
    if (regularPath.empty()) {
        static const char* kCandidates[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        };
        for (const char* path : kCandidates) {
            if (fs::exists(path)) {
                regularPath = path;
                boldPath = siblingFontPath(path, {"Bold", "b", "B"});
                italicPath =
                    siblingFontPath(path, {"Oblique", "Italic", "Obl"});
                boldItalicPath = siblingFontPath(
                    path, {"BoldOblique", "BoldItalic"});
                break;
            }
        }
    }

    if (!regularPath.empty()) {
        g_fonts.regular = addFont(io, regularPath, 18.0f);
        g_fonts.bold = addFont(io, boldPath, 18.0f);
        if (!g_fonts.bold) {
            g_fonts.bold = g_fonts.regular;
        }
        g_fonts.italic = addFont(io, italicPath, 18.0f);
        if (!g_fonts.italic) {
            g_fonts.italic = g_fonts.regular;
        }
        g_fonts.boldItalic = addFont(io, boldItalicPath, 18.0f);
        if (!g_fonts.boldItalic || g_fonts.boldItalic == g_fonts.italic) {
            g_fonts.boldItalic = g_fonts.bold;
        }
        g_fonts.h1 = addFont(io, boldPath, 22.0f);
        if (!g_fonts.h1) {
            g_fonts.h1 = g_fonts.bold;
        }
        return;
    }

    // 3. Embedded font fallback (scaled up).
    io.Fonts->AddFontDefault();
    io.FontGlobalScale = 1.4f;
    g_fonts.regular = ImGui::GetFont();
    g_fonts.bold = ImGui::GetFont();
    g_fonts.italic = ImGui::GetFont();
    g_fonts.boldItalic = ImGui::GetFont();
    g_fonts.h1 = ImGui::GetFont();
}

}  // namespace ab