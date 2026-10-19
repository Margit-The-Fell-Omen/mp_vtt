#include "notes/MarkdownRenderer.h"

#include <glad/glad.h>

#include <imgui.h>

#include <md4c.h>
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

// A cached GPU texture for an inline image.
struct ImageCacheEntry {
    ImTextureID texture = 0;
    int width = 0;
    int height = 0;
};

// Resolves an image URL (possibly relative to baseDir) and returns a cached
// ImGui texture + natural dimensions. Returns nullptr on failure.
ImageCacheEntry* loadImageCache(const std::string& rawUrl,
                                const fs::path& baseDir) {
    static std::unordered_map<std::string, ImageCacheEntry> cache;

    // Resolve the URL to a filesystem path.
    std::string key = rawUrl;
    fs::path path;
    if (rawUrl.empty()) {
        return nullptr;
    }
    if (rawUrl.rfind("http://", 0) == 0 || rawUrl.rfind("https://", 0) == 0 ||
        rawUrl.rfind("data:", 0) == 0 || rawUrl.rfind("file://", 0) == 0) {
        return nullptr;  // remote/data URLs unsupported for now
    }

    // Strip a URL fragment/query (rare in markdown, but be safe).
    std::string cleaned = rawUrl;
    if (auto hash = cleaned.find('#'); hash != std::string::npos) {
        cleaned.erase(hash);
    }

    fs::path p(cleaned);
    if (p.is_relative()) {
        if (baseDir.empty()) {
            return nullptr;
        }
        path = baseDir / p;
    } else {
        path = p;
    }
    key = path.string();

    auto it = cache.find(key);
    if (it != cache.end()) {
        return &it->second;
    }

    int w = 0;
    int h = 0;
    int comp = 0;
    stbi_set_flip_vertically_on_load(0);  // ImGui expects top-left origin
    stbi_uc* data = stbi_load(key.c_str(), &w, &h, &comp, 4);
    if (!data || w <= 0 || h <= 0) {
        if (data) {
            stbi_image_free(data);
        }
        // Cache a negative result implicitly by not inserting; cheap enough.
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

// Per-block rendering state stack entry.
struct BlockState {
    MD_BLOCKTYPE type = MD_BLOCK_DOC;
    unsigned hLevel = 0;        // for H blocks
    bool li = false;            // inside a list item
    bool code = false;          // inside a code block (monospace, verbatim)
    bool quote = false;         // inside a blockquote
    bool table = false;         // inside a table
    bool td = false;            // inside a table cell
};

struct RenderContext {
    std::vector<BlockState> stack;
    fs::path baseDir;
    bool firstTextOfParagraph = false;
    bool spanStrong = false;
    bool spanEm = false;
    bool spanCode = false;
    bool spanDel = false;
    bool spanUnderline = false;
    bool linkActive = false;
    int inImage = 0;            // >0 while inside an <img> span (suppress alt)
    std::string linkHref;
};

int enterBlock(MD_BLOCKTYPE type, void* detail, void* userdata) {
    auto* ctx = static_cast<RenderContext*>(userdata);
    BlockState st;
    st.type = type;

    switch (type) {
        case MD_BLOCK_H: {
            auto* d = static_cast<MD_BLOCK_H_DETAIL*>(detail);
            st.hLevel = d->level;
            break;
        }
        case MD_BLOCK_CODE:
        case MD_BLOCK_HTML:
            st.code = true;
            break;
        case MD_BLOCK_QUOTE:
            st.quote = true;
            break;
        case MD_BLOCK_TABLE:
        case MD_BLOCK_THEAD:
        case MD_BLOCK_TBODY:
            st.table = true;
            break;
        case MD_BLOCK_TH:
        case MD_BLOCK_TD:
            st.td = true;
            break;
        case MD_BLOCK_LI:
            st.li = true;
            break;
        default:
            break;
    }

    ctx->stack.push_back(st);

    switch (type) {
        case MD_BLOCK_P:
            ctx->firstTextOfParagraph = true;
            break;
        case MD_BLOCK_UL:
        case MD_BLOCK_OL:
            ImGui::Indent();
            break;
        case MD_BLOCK_LI:
            ImGui::Bullet();
            ImGui::SameLine();
            break;
        case MD_BLOCK_HR:
            ImGui::Separator();
            break;
        case MD_BLOCK_H: {
            // Distinguish heading levels via color.
            const ImVec4 levels[] = {
                ImVec4(1.00f, 0.95f, 0.80f, 1.0f),
                ImVec4(0.95f, 0.90f, 0.70f, 1.0f),
                ImVec4(0.90f, 0.85f, 0.62f, 1.0f),
                ImVec4(0.85f, 0.82f, 0.62f, 1.0f),
                ImVec4(0.80f, 0.80f, 0.65f, 1.0f),
                ImVec4(0.75f, 0.78f, 0.68f, 1.0f),
            };
            int idx = static_cast<int>(st.hLevel);
            if (idx < 1) idx = 1;
            if (idx > 6) idx = 6;
            ImGui::PushStyleColor(ImGuiCol_Text, levels[idx - 1]);
            break;
        }
        default:
            break;
    }

    return 0;
}

int leaveBlock(MD_BLOCKTYPE type, void* /*detail*/, void* userdata) {
    auto* ctx = static_cast<RenderContext*>(userdata);

    switch (type) {
        case MD_BLOCK_UL:
        case MD_BLOCK_OL:
            ImGui::Unindent();
            break;
        case MD_BLOCK_H:
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, 4));
            break;
        case MD_BLOCK_P:
        case MD_BLOCK_QUOTE:
            ImGui::Dummy(ImVec2(0, 4));
            break;
        default:
            break;
    }

    if (!ctx->stack.empty()) {
        ctx->stack.pop_back();
    }
    return 0;
}

int enterSpan(MD_SPANTYPE type, void* detail, void* userdata) {
    auto* ctx = static_cast<RenderContext*>(userdata);

    switch (type) {
        case MD_SPAN_STRONG:
            ctx->spanStrong = true;
            break;
        case MD_SPAN_EM:
            ctx->spanEm = true;
            break;
        case MD_SPAN_CODE:
            ctx->spanCode = true;
            break;
        case MD_SPAN_DEL:
            ctx->spanDel = true;
            break;
        case MD_SPAN_U:
            ctx->spanUnderline = true;
            break;
        case MD_SPAN_A: {
            auto* d = static_cast<MD_SPAN_A_DETAIL*>(detail);
            ctx->linkActive = true;
            ctx->linkHref.assign(d->href.text, d->href.size);
            break;
        }
        case MD_SPAN_IMG: {
            auto* d = static_cast<MD_SPAN_IMG_DETAIL*>(detail);
            ctx->inImage++;
            std::string src(d->src.text, d->src.size);
            ImageCacheEntry* img = loadImageCache(src, ctx->baseDir);
            if (img) {
                // Fit to a max on-screen size while preserving aspect ratio.
                const float maxW = 480.0f;
                const float maxH = 360.0f;
                float scale = 1.0f;
                if (img->width > maxW || img->height > maxH) {
                    scale = std::min(maxW / img->width, maxH / img->height);
                }
                ImVec2 size(img->width * scale, img->height * scale);
                ImGui::Image(img->texture, size);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s (%dx%d)", src.c_str(), img->width,
                                      img->height);
                }
            } else {
                ImGui::TextDisabled("(image: %s)", src.c_str());
            }
            break;
        }
        default:
            break;
    }
    return 0;
}

int leaveSpan(MD_SPANTYPE type, void* /*detail*/, void* userdata) {
    auto* ctx = static_cast<RenderContext*>(userdata);

    switch (type) {
        case MD_SPAN_STRONG:
            ctx->spanStrong = false;
            break;
        case MD_SPAN_EM:
            ctx->spanEm = false;
            break;
        case MD_SPAN_CODE:
            ctx->spanCode = false;
            break;
        case MD_SPAN_DEL:
            ctx->spanDel = false;
            break;
        case MD_SPAN_U:
            ctx->spanUnderline = false;
            break;
        case MD_SPAN_A:
            ctx->linkActive = false;
            ctx->linkHref.clear();
            break;
        case MD_SPAN_IMG:
            if (ctx->inImage > 0) {
                ctx->inImage--;
            }
            break;
        default:
            break;
    }
    return 0;
}

int textCallback(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size,
                 void* userdata) {
    auto* ctx = static_cast<RenderContext*>(userdata);

    // Inside an image, the emitted text is the alt attribute; drop it.
    if (ctx->inImage > 0) {
        return 0;
    }

    if (type == MD_TEXT_BR) {
        ImGui::NewLine();
        return 0;
    }
    if (type == MD_TEXT_SOFTBR) {
        // soft break -> space in rendered output inline
        ImGui::TextUnformatted(" ");
        return 0;
    }

    std::string s(text, size);
    if (s.empty()) {
        return 0;
    }

    // Fragment styling: rebuild with decorations by wrapping the string.
    // For simplicity we push style color modifiers around the segment.
    bool isCode = ctx->spanCode ||
                  std::any_of(ctx->stack.begin(), ctx->stack.end(),
                              [](const BlockState& b) { return b.code; });

    if (isCode) {
        ImGui::TextUnformatted(s.c_str());
        return 0;
    }

    // Strong: brighter text; Em: italic color; Del: strikethrough color;
    // U: underline color; Link: blue text.
    if (ctx->linkActive) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.60f, 0.95f, 1.0f));
        ImGui::TextUnformatted(s.c_str());
        ImGui::PopStyleColor();
    } else if (ctx->spanStrong) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.35f, 1.0f));
        ImGui::TextUnformatted(s.c_str());
        ImGui::PopStyleColor();
    } else if (ctx->spanEm) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.80f, 0.55f, 1.0f));
        ImGui::TextUnformatted(s.c_str());
        ImGui::PopStyleColor();
    } else if (ctx->spanDel) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
        ImGui::TextUnformatted(s.c_str());
        ImGui::PopStyleColor();
    } else if (ctx->spanUnderline) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.95f, 1.0f));
        ImGui::TextUnformatted(s.c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::TextUnformatted(s.c_str());
    }

    (void)ctx;
    return 0;
}

}  // namespace

void MarkdownRenderer::render(const std::string& markdown,
                              const fs::path& baseDir) {
    RenderContext ctx;
    ctx.baseDir = baseDir;

    MD_PARSER parser{};
    parser.abi_version = 0;
    parser.flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS |
                   MD_FLAG_UNDERLINE | MD_FLAG_COLLAPSEWHITESPACE |
                   MD_FLAG_NOHTMLBLOCKS | MD_FLAG_NOHTMLSPANS;
    parser.enter_block = enterBlock;
    parser.leave_block = leaveBlock;
    parser.enter_span = enterSpan;
    parser.leave_span = leaveSpan;
    parser.text = textCallback;
    parser.debug_log = nullptr;
    parser.syntax = nullptr;

    md_parse(markdown.c_str(), static_cast<MD_SIZE>(markdown.size()), &parser,
             &ctx);
}

}  // namespace ab