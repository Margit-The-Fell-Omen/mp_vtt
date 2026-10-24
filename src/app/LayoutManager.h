#pragma once

#include <imgui.h>

#include <string>
#include <unordered_set>
#include <vector>

namespace ab {

// Governs the ImGui dock tree: enforces that no layout exceeds the app
// screen, provides vim-style keyboard layout commands, and keeps the panels
// open/closed state. Drag & drop docking is left untouched.
class LayoutManager {
public:
    enum PendingOp {
        PendingOp_None = 0,
        PendingOp_Default,
        PendingOp_Move,
        PendingOp_Maximize,
        PendingOp_Equalize,
    };

    struct Pending {
        PendingOp op = PendingOp_None;
        ImGuiDir dir = ImGuiDir_None;
        float ratio = 0.5f;
    };

    // Registers an app window (dock-able panel) by title.
    void registerWindow(const char* name);

    // Builds the default grid layout (also used by "r"). Re-docks all open
    // windows by name.
    void applyDefaultLayout(ImGuiID dockId, const ImVec2& hostSize);

    // Executes a queued layout command (queued by handleKeys). Call at the
    // start of a frame, right after DockSpace().
    void applyPending(ImGuiID dockId, const ImVec2& hostSize);

    // Clamps floating windows back into the host work rect and repairs
    // degenerate dock nodes. Call once per frame after DockSpace().
    void enforceBounds(ImGuiID dockId, const ImVec2& hostPos,
                       const ImVec2& hostSize);

    // Processes keyboard layout commands (1/2/3 select panels; movement keys
    // act while a top-level panel is focused). Call once per frame.
    void handleKeys(ImGuiID dockId, const ImVec2& hostSize);

    // Renders the keymap hint overlay while a top-level panel is focused.
    void renderOverlay();

    // Window visibility ("x" closes the focused window; reset reopens all).
    bool isWindowOpen(const char* name) const {
        return m_closed.find(name) == m_closed.end();
    }
    void setWindowOpen(const char* name, bool open);
    void openAll();

private:
    std::string focusedWindowName() const;
    void        focusPanelIndex(size_t index);
    void        dockFocusedTo(ImGuiID dockId, const ImVec2& hostSize,
                              ImGuiDir dir, float ratio);
    void        resetToDefault(ImGuiID dockId, const ImVec2& hostSize);
    void        maximize(ImGuiID dockId);
    void        equalize(ImGuiID dockId);
    void        focusDirectional(ImGuiID dockId, ImGuiDir dir);
    void        rebuild(ImGuiID dockId, const ImVec2& hostSize,
                        const std::string& focusName, ImGuiDir dir,
                        float ratio);

    std::vector<std::string> m_names;      // registered window names
    std::unordered_set<std::string> m_closed;
    std::string m_lastFocused;             // last focused top-level panel

    Pending m_pending;
    bool m_pendingSet = false;
};

}  // namespace ab