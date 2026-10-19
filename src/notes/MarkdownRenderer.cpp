#include "notes/MarkdownRenderer.h"

#include <imgui.h>

#include <md4c.h>

#include <algorithm>
#include <string>
#include <vector>

namespace ab {

namespace {

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
    bool firstTextOfParagraph = false;
    bool spanStrong = false;
    bool spanEm = false;
    bool spanCode = false;
    bool spanDel = false;
    bool spanUnderline = false;
    bool linkActive = false;
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
            std::string alt;
            // alt is rendered via nested text callbacks; here we only need src.
            std::string src(d->src.text, d->src.size);
            ImGui::TextDisabled("(image: %s)", src.c_str());
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
        default:
            break;
    }
    return 0;
}

int textCallback(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size,
                 void* userdata) {
    auto* ctx = static_cast<RenderContext*>(userdata);

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

void MarkdownRenderer::render(const std::string& markdown) {
    RenderContext ctx;

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