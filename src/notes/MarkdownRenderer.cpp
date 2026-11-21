#include "notes/MarkdownRenderer.h"

#include "app/Theme.h"
#include "core/Storage.h"
#include "imgui_md.h"
#include "platform/Platform.h"

#include "platform/GL.h"
#include <stb_image.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace ab {

namespace fs = std::filesystem;

namespace {

bool isExternalUrl(const std::string& href) {
    return href.rfind("http://", 0) == 0 || href.rfind("https://", 0) == 0 ||
           href.rfind("mailto:", 0) == 0;
}

// A cached GPU texture for an inline image.
struct ImageCacheEntry {
    ImTextureID texture = 0;
    int width = 0;
    int height = 0;
};

ImageCacheEntry* loadImageCache(const std::string& rawUrl,
                                const fs::path& baseDir,
                                Storage& storage) {
    static std::unordered_map<std::string, ImageCacheEntry> cache;

    if (rawUrl.empty()) {
        return nullptr;
    }
    if (rawUrl.rfind("http://", 0) == 0 || rawUrl.rfind("https://", 0) == 0 ||
        rawUrl.rfind("data:", 0) == 0 || rawUrl.rfind("file://", 0) == 0) {
        return nullptr;
    }

    std::string cleaned = rawUrl;
    if (auto hash = cleaned.find('#'); hash != std::string::npos) {
        cleaned.erase(hash);
    }

    fs::path p = platform::pathFromUtf8(cleaned);
    Storage::Path path = p.is_relative()
                             ? (baseDir.empty() ? Storage::Path()
                                                : storage.join(baseDir, p))
                             : p;
    std::string key = platform::pathToUtf8(path);

    auto it = cache.find(key);
    if (it != cache.end()) {
        return &it->second;
    }

    std::vector<unsigned char> bytes;
    if (!storage.readBytes(path, bytes) || bytes.empty()) {
        return nullptr;
    }

    int w = 0;
    int h = 0;
    int comp = 0;
    stbi_set_flip_vertically_on_load(0);
    stbi_uc* data = stbi_load_from_memory(
        bytes.data(), static_cast<int>(bytes.size()), &w, &h, &comp, 4);
    if (!data || w <= 0 || h <= 0) {
        if (data) {
            stbi_image_free(data);
        }
        return nullptr;
    }

    unsigned int gltex = 0;
    glGenTextures(1, &gltex);
    glBindTexture(GL_TEXTURE_2D, gltex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 data);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    ImageCacheEntry entry;
    entry.texture = static_cast<ImTextureID>(gltex);
    entry.width = w;
    entry.height = h;
    cache[key] = entry;
    return &cache[key];
}

// Renderer implementation built on imgui_md (MD4C-backed).
class Renderer final : public imgui_md {
public:
    static void Render(const std::string& markdown, const fs::path& baseDir,
                       Storage& storage) {
        static Renderer self;
        self.m_baseDir = baseDir;
        self.m_storage = &storage;
        self.m_cachedFonts = ab::fonts();
        self.print(markdown.c_str(), markdown.c_str() + markdown.size());
    }

protected:
    ImFont* get_font() const override {
        const FontSet& f = m_cachedFonts;
        ImFont* reg = f.regular ? f.regular : ImGui::GetFont();
        ImFont* bold = f.bold ? f.bold : reg;
        ImFont* italic = f.italic ? f.italic : reg;
        ImFont* boldItalic = f.boldItalic ? f.boldItalic : bold;
        if (boldItalic == italic) {
            boldItalic = bold;  // a real BI font is required for ***style***
        }
        ImFont* h1 = f.h1 ? f.h1 : bold;
        if (m_is_em) {
            return m_is_strong ? boldItalic : italic;
        }
        if (m_hlevel >= 1) {
            return h1;
        }
        if (m_is_strong || m_is_table_header) {
            return bold;
        }
        return reg;
    }

    ImVec4 get_color() const override {
        if (!m_href.empty()) {
            return ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered];
        }
        return ImGui::GetStyle().Colors[ImGuiCol_Text];
    }

    bool get_image(image_info& nfo) const override {
        ImageCacheEntry* e =
            m_storage ? loadImageCache(m_href, m_baseDir, *m_storage)
                      : nullptr;
        if (!e || e->width <= 0 || e->height <= 0) {
            return false;
        }
        // Cap the on-screen size while preserving aspect ratio. imgui_md
        // will also shrink images wider than the available region.
        const float maxW = 480.0f;
        const float maxH = 360.0f;
        float scale = 1.0f;
        if (e->width > maxW || e->height > maxH) {
            scale = std::min(maxW / e->width, maxH / e->height);
        }
        nfo.texture_id = e->texture;
        nfo.size = ImVec2(e->width * scale, e->height * scale);
        nfo.uv0 = ImVec2(0.0f, 0.0f);
        nfo.uv1 = ImVec2(1.0f, 1.0f);
        nfo.col_tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        nfo.col_border = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        return true;
    }

    void open_url() const override {
        if (!isExternalUrl(m_href)) {
            return;
        }
        platform::openUrl(m_href);
    }

    // A single source newline inside a paragraph (soft break) should be
    // visible; the default imgui_md implementation drops it entirely.
    void soft_break() override {
        ImGui::NewLine();
    }

    void SPAN_CODE(bool e) override {
        ImFont* mono =
            m_cachedFonts.mono ? m_cachedFonts.mono : ImGui::GetFont();
        if (e) {
            ImGui::PushFont(mono);
            ImGui::PushStyleColor(ImGuiCol_Text, ab::accentColor());
        } else {
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }
    }

    void BLOCK_CODE(const MD_BLOCK_CODE_DETAIL*, bool e) override {
        ImFont* mono =
            m_cachedFonts.mono ? m_cachedFonts.mono : ImGui::GetFont();
        if (e) {
            m_is_code = true;
            ImGui::PushFont(mono);
            ImGui::PushStyleColor(ImGuiCol_Text, ab::accentColor());
            ImGui::NewLine();
            ImGui::Indent();
        } else {
            ImGui::Unindent();
            ImGui::PopStyleColor();
            ImGui::PopFont();
            m_is_code = false;
        }
    }

private:
    mutable fs::path m_baseDir;
    mutable FontSet m_cachedFonts;
    Storage* m_storage = nullptr;
};

}  // namespace

void MarkdownRenderer::render(const std::string& markdown,
                              const fs::path& baseDir, Storage& storage) {
    Renderer::Render(markdown, baseDir, storage);
}

}  // namespace ab