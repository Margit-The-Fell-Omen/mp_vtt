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

ImageCacheEntry* loadImageCache(const std::string& rawUrl,
                                const fs::path& baseDir) {
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

    fs::path p(cleaned);
    fs::path path = p.is_relative() ? (baseDir.empty() ? fs::path() : baseDir / p)
                                    : p;
    std::string key = path.string();

    auto it = cache.find(key);
    if (it != cache.end()) {
        return &it->second;
    }
    if (!fs::exists(path)) {
        return nullptr;
    }

    int w = 0;
    int h = 0;
    int comp = 0;
    stbi_set_flip_vertically_on_load(0);
    stbi_uc* data = stbi_load(key.c_str(), &w, &h, &comp, 4);
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

// ---- Inline text accumulation -------------------------------------------
//
// We accumulate inline content into a plain string (with embedded color
// markers) so that a single TextWrapped() call handles soft wrapping across
// a whole paragraph. This avoids the buggy per-fragment TextUnformatted
// layout that caused list items and following text to render on one line.

constexpr char kColorMark = '\x01';  // sentinel: next char selects a style

enum ColorMode {
    kNormal = 0,
    kStrong = 1,
    kEm = 2,
    kCode = 3,
    kLink = 4,
    kDel = 5,
    kUnderline = 6,
};

struct BlockState {
    MD_BLOCKTYPE type = MD_BLOCK_DOC;
    unsigned hLevel = 0;
    int listLevel = 0;       // nesting depth of lists
    int counter = 0;         // running item number for ordered lists
    bool li = false;         // inside a list item
    bool inCode = false;     // inside a fenced code block
    bool inQuote = false;    // inside a blockquote
    bool tableCell = false;  // inside a table cell
    int compact = 0;         // push count of colors (compact vs loose)
};

struct ListItemState {
    bool ordered = false;
    int number = 1;
    bool hasContent = false;
};

struct RenderContext {
    fs::path baseDir;

    // Inline accumulation buffer for the current paragraph.
    std::string buf;

    // Span nesting flags (for picking a color mode when text arrives).
    bool spanStrong = false;
    bool spanEm = false;
    bool spanCode = false;
    bool spanDel = false;
    bool spanUnderline = false;
    bool linkActive = false;

    int inImage = 0;      // >0 while inside an <img> span

    // Block structure.
    std::vector<BlockState> stack;
    std::vector<ListItemState> liStack;
    bool startOfPara = true;
    ColorMode lastMode = kNormal;
    bool markerPending = false;   // a list marker is awaiting emission

    // Helpers.
    bool inList() const {
        for (const auto& b : stack) {
            if (b.type == MD_BLOCK_UL || b.type == MD_BLOCK_OL) {
                return true;
            }
        }
        return false;
    }
    BlockState* lastList() {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            if (it->type == MD_BLOCK_UL || it->type == MD_BLOCK_OL) {
                return &*it;
            }
        }
        return nullptr;
    }
    ColorMode currentColorMode() const {
        if (spanCode) return kCode;
        if (linkActive) return kLink;
        if (spanStrong) return kStrong;
        if (spanEm) return kEm;
        if (spanDel) return kDel;
        if (spanUnderline) return kUnderline;
        return kNormal;
    }
};

// ---------- rendering helpers -------------------------------------------

ImVec4 colorForMode(ColorMode m) {
    switch (m) {
        case kStrong:     return ImVec4(1.00f, 0.85f, 0.35f, 1.0f);
        case kEm:         return ImVec4(0.55f, 0.80f, 0.55f, 1.0f);
        case kLink:       return ImVec4(0.35f, 0.60f, 0.95f, 1.0f);
        case kDel:        return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
        case kUnderline:  return ImVec4(0.75f, 0.75f, 0.95f, 1.0f);
        case kCode:       return ImVec4(0.76f, 0.80f, 0.86f, 1.0f);
        case kNormal:
        default:          return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

// Renders the accumulated buffer to ImGui with soft wrapping and colors.
void renderBuffer(RenderContext& ctx, bool indent) {
    (void)indent;
    if (ctx.buf.empty()) {
        return;
    }
    // Remove a trailing softbreak space we might have appended.
    if (!ctx.buf.empty() && ctx.buf.back() == ' ') {
        ctx.buf.pop_back();
    }
    if (ctx.buf.empty()) {
        return;
    }

    // Walk color-marked runs and emit TextWrapped segments.
    std::string run;
    ColorMode current = kNormal;
    auto emitRun = [&]() {
        if (run.empty()) {
            return;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, colorForMode(current));
        ImGui::TextWrapped("%s", run.c_str());
        ImGui::PopStyleColor();
        run.clear();
    };

    for (size_t i = 0; i < ctx.buf.size(); ++i) {
        char c = ctx.buf[i];
        if (c == kColorMark && i + 1 < ctx.buf.size()) {
            emitRun();
            current = static_cast<ColorMode>(ctx.buf[++i]);
        } else if (c == '\n') {
            emitRun();
            ImGui::NewLine();
        } else {
            run.push_back(c);
        }
    }
    emitRun();
    ctx.buf.clear();
}

void openParagraph(RenderContext& ctx) {
    ctx.buf.clear();
    ctx.startOfPara = true;
    ctx.lastMode = kNormal;
}

// Emits a pending list-item marker into the buffer (bullet or number).
void emitMarker(RenderContext& ctx) {
    if (!ctx.markerPending) {
        return;
    }
    ctx.markerPending = false;

    bool ordered = false;
    int number = 1;
    if (!ctx.liStack.empty()) {
        ordered = ctx.liStack.back().ordered;
        number = ctx.liStack.back().number;
        ctx.liStack.back().hasContent = true;
    }
    // The marker is plain text (no color mark), so it stays on the same
    // wrapped line as the text that follows.
    if (ordered) {
        ctx.buf.append(std::to_string(number));
        ctx.buf.append(". ");
    } else {
        ctx.buf.append("\xE2\x80\xA2 ");  // "• "
    }
}

void closeParagraph(RenderContext& ctx) {
    renderBuffer(ctx, ctx.inList());
    ctx.buf.clear();
    ctx.startOfPara = true;
    ctx.lastMode = kNormal;
    // Paragraph spacing after a block.
    if (!ctx.inList()) {
        ImGui::Dummy(ImVec2(0, 4));
    }
}

// ---------- callbacks ----------------------------------------------------

int enterBlock(MD_BLOCKTYPE type, void* detail, void* userdata) {
    auto* ctx = static_cast<RenderContext*>(userdata);
    BlockState st;
    st.type = type;

    switch (type) {
        case MD_BLOCK_H:
            st.hLevel = static_cast<MD_BLOCK_H_DETAIL*>(detail)->level;
            break;
        case MD_BLOCK_CODE:
        case MD_BLOCK_HTML:
            st.inCode = true;
            break;
        case MD_BLOCK_QUOTE:
            st.inQuote = true;
            break;
        case MD_BLOCK_TH:
        case MD_BLOCK_TD:
            st.tableCell = true;
            break;
        case MD_BLOCK_LI:
            st.li = true;
            if (!ctx->stack.empty()) {
                st.listLevel = 1;
            }
            break;
        default:
            break;
    }

    // Track list nesting for indentation.
    bool isList = (type == MD_BLOCK_UL || type == MD_BLOCK_OL);
    if (isList) {
        int depth = 0;
        for (const auto& b : ctx->stack) {
            if (b.type == MD_BLOCK_UL || b.type == MD_BLOCK_OL) {
                ++depth;
            }
        }
        st.listLevel = depth + 1;
    }

    ctx->stack.push_back(st);

    switch (type) {
        case MD_BLOCK_P:
        case MD_BLOCK_H:
            openParagraph(*ctx);
            emitMarker(*ctx);
            break;
        case MD_BLOCK_UL:
        case MD_BLOCK_OL:
            ImGui::Indent();
            break;
        case MD_BLOCK_LI: {
            // Determine whether the parent list is ordered, and bump its
            // running counter for numbered items.
            bool ordered = false;
            int number = 1;
            for (auto it = ctx->stack.rbegin(); it != ctx->stack.rend(); ++it) {
                if (it->type == MD_BLOCK_OL) {
                    ordered = true;
                    number = ++(it->counter);
                    break;
                }
                if (it->type == MD_BLOCK_UL) {
                    break;
                }
            }
            ctx->liStack.push_back(ListItemState{ordered, number, false});
            ctx->markerPending = true;
            break;
        }
        case MD_BLOCK_CODE:
            ImGui::Indent();
            break;
        case MD_BLOCK_QUOTE:
            ImGui::Indent();
            break;
        case MD_BLOCK_HR:
            closeParagraph(*ctx);
            ImGui::Separator();
            break;
        case MD_BLOCK_TABLE:
            ImGui::Separator();
            break;
        default:
            break;
    }

    return 0;
}

int leaveBlock(MD_BLOCKTYPE type, void* detail, void* userdata) {
    auto* ctx = static_cast<RenderContext*>(userdata);
    (void)detail;

    switch (type) {
        case MD_BLOCK_P:
        case MD_BLOCK_H:
        case MD_BLOCK_QUOTE:
            closeParagraph(*ctx);
            break;
        case MD_BLOCK_LI:
            closeParagraph(*ctx);
            if (!ctx->liStack.empty()) {
                ctx->liStack.pop_back();
            }
            break;
        case MD_BLOCK_UL:
        case MD_BLOCK_OL:
            ImGui::Unindent();
            break;
        case MD_BLOCK_CODE:
            ImGui::Unindent();
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
        case MD_SPAN_STRONG: ctx->spanStrong = true; break;
        case MD_SPAN_EM:     ctx->spanEm = true; break;
        case MD_SPAN_CODE:   ctx->spanCode = true; break;
        case MD_SPAN_DEL:    ctx->spanDel = true; break;
        case MD_SPAN_U:      ctx->spanUnderline = true; break;
        case MD_SPAN_A:
            ctx->linkActive = true;
            break;
        case MD_SPAN_IMG: {
            ctx->inImage++;
            auto* d = static_cast<MD_SPAN_IMG_DETAIL*>(detail);
            std::string src(d->src.text, d->src.size);

            // Flush any pending inline text so the image sits on its own line.
            if (!ctx->buf.empty()) {
                renderBuffer(*ctx, ctx->inList());
            }

            ImageCacheEntry* img = loadImageCache(src, ctx->baseDir);
            if (img) {
                const float maxW = 480.0f;
                const float maxH = 360.0f;
                float scale = std::min(1.0f,
                    std::min(maxW / img->width, maxH / img->height));
                ImGui::Image(img->texture,
                              ImVec2(img->width * scale, img->height * scale));
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
        case MD_SPAN_STRONG: ctx->spanStrong = false; break;
        case MD_SPAN_EM:     ctx->spanEm = false; break;
        case MD_SPAN_CODE:   ctx->spanCode = false; break;
        case MD_SPAN_DEL:    ctx->spanDel = false; break;
        case MD_SPAN_U:      ctx->spanUnderline = false; break;
        case MD_SPAN_A:      ctx->linkActive = false; break;
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

    if (ctx->inImage > 0) {
        return 0;
    }

    if (type == MD_TEXT_BR) {
        emitMarker(*ctx);
        ctx->buf.push_back('\n');
        return 0;
    }
    if (type == MD_TEXT_SOFTBR) {
        emitMarker(*ctx);
        ctx->buf.push_back(' ');
        return 0;
    }

    // For tight lists (no paragraph block), emit the marker on first text.
    emitMarker(*ctx);

    std::string s(text, size);

    // A heading opens with its color applied for the whole line.
    bool inHeading = !ctx->stack.empty() &&
                     ctx->stack.back().type == MD_BLOCK_H;

    ColorMode mode = ctx->currentColorMode();

    // Apply heading styling as a leading color mark at paragraph start.
    if (inHeading && ctx->startOfPara) {
        static const ColorMode headingModes[] = {
            kStrong, kStrong, kStrong, kStrong, kStrong, kStrong,
        };
        unsigned lvl = ctx->stack.back().hLevel;
        if (lvl >= 1 && lvl <= 6) {
            mode = headingModes[lvl - 1];
        }
    }

    // Emit a color mark if the mode changed. We use the paragraph-start flag
    // to seed the initial mode.
    if (ctx->buf.empty()) {
        ctx->buf.push_back(kColorMark);
        ctx->buf.push_back(static_cast<char>(mode));
        ctx->startOfPara = false;
        ctx->lastMode = mode;
    } else if (ctx->lastMode != mode) {
        ctx->buf.push_back(kColorMark);
        ctx->buf.push_back(static_cast<char>(mode));
        ctx->lastMode = mode;
    }

    ctx->buf.append(s);
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

    // Flush any trailing content.
    renderBuffer(ctx, ctx.inList());
}

}  // namespace ab