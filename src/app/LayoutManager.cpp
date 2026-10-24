#include "app/LayoutManager.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ab {

namespace {

bool isRegistered(const std::vector<std::string>& names, const char* name) {
    if (!name) {
        return false;
    }
    return std::find(names.begin(), names.end(), name) != names.end();
}
}  // namespace

void LayoutManager::registerWindow(const char* name) {
    if (std::find(m_names.begin(), m_names.end(), name) == m_names.end()) {
        m_names.push_back(name);
    }
}

void LayoutManager::setWindowOpen(const char* name, bool open) {
    if (open) {
        m_closed.erase(name);
    } else {
        m_closed.insert(name);
    }
}

void LayoutManager::openAll() {
    m_closed.clear();
}

// ---------------------------------------------------------------- rebuild --

void LayoutManager::rebuild(ImGuiID dockId, const ImVec2& hostSize,
                            const std::string& focusName, ImGuiDir dir,
                            float ratio) {
    (void)ratio;
    ImGui::DockBuilderRemoveNode(dockId);
    ImGuiID root =
        ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(root, hostSize);

    if (dir == ImGuiDir_None) {
        // Default: left column (Notes top, Initiative bottom), Projection right.
        ImGuiID left = 0;
        ImGuiID right = 0;
        ImGui::DockBuilderSplitNode(root, ImGuiDir_Left, 0.64f, &left, &right);
        ImGuiID top = 0;
        ImGuiID bottom = 0;
        ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.55f, &top, &bottom);

        std::string target = focusName;
        if (target.empty()) {
            for (const auto& n : m_names) {
                if (isWindowOpen(n.c_str())) {
                    target = n;
                    break;
                }
            }
        }
        ImGui::DockBuilderDockWindow(target.c_str(), top);
        // Fill the remaining slots: next open window to bottom, then right.
        bool placedBottom = false;
        for (const auto& n : m_names) {
            if (!isWindowOpen(n.c_str()) || n == target) {
                continue;
            }
            if (!placedBottom) {
                ImGui::DockBuilderDockWindow(n.c_str(), bottom);
                placedBottom = true;
            } else {
                ImGui::DockBuilderDockWindow(n.c_str(), right);
            }
        }
    } else {
        // Move window to an edge: focus window on the target side, the rest
        // on the opposite side (tab group).
        ImGuiID atDir = 0;
        ImGuiID atOpp = 0;
        ImGui::DockBuilderSplitNode(root, dir, 0.5f, &atDir, &atOpp);

        std::string target = focusName;
        bool focusedPlaced = false;
        for (const auto& n : m_names) {
            if (!isWindowOpen(n.c_str())) {
                continue;
            }
            if (!focusedPlaced && n == target) {
                ImGui::DockBuilderDockWindow(n.c_str(), atDir);
                focusedPlaced = true;
            } else {
                ImGui::DockBuilderDockWindow(n.c_str(), atOpp);
                // ensure a window exists on the focus side even if target
                // was closed/unset
                if (!focusedPlaced) {
                    focusedPlaced = true;
                }
            }
        }
        if (!focusedPlaced) {
            for (const auto& n : m_names) {
                if (isWindowOpen(n.c_str())) {
                    ImGui::DockBuilderDockWindow(n.c_str(), atDir);
                    break;
                }
            }
        }
    }
    ImGui::DockBuilderFinish(dockId);
}

void LayoutManager::applyDefaultLayout(ImGuiID dockId, const ImVec2& hostSize) {
    rebuild(dockId, hostSize, "", ImGuiDir_None, 0.5f);
}

void LayoutManager::resetToDefault(ImGuiID dockId, const ImVec2& hostSize) {
    openAll();
    applyDefaultLayout(dockId, hostSize);
}

// ------------------------------------------------------------- frame logic --

void LayoutManager::applyPending(ImGuiID dockId, const ImVec2& hostSize) {
    if (!m_pendingSet) {
        return;
    }
    const Pending todo = m_pending;
    m_pendingSet = false;
    m_pending = Pending{};

    switch (todo.op) {
        case PendingOp_None:
            break;
        case PendingOp_Default:
            resetToDefault(dockId, hostSize);
            break;
        case PendingOp_Move:
            dockFocusedTo(dockId, hostSize, todo.dir, todo.ratio);
            break;
        case PendingOp_Maximize:
            maximize(dockId);
            break;
        case PendingOp_Equalize:
            equalize(dockId);
            break;
    }
}

void LayoutManager::enforceBounds(ImGuiID dockId, const ImVec2& hostPos,
                                  const ImVec2& hostSize) {
    ImGuiDockNode* root = ImGui::DockBuilderGetNode(dockId);
    if (root) {
        // Repair a degenerate root (e.g. stale ini) so the whole tree fits.
        if (root->Size.x <= 0 || root->Size.y <= 0) {
            root->Pos = hostPos;
            root->Size = hostSize;
        }
    }

    // Clamp floating (undocked) app windows back inside the host rect.
    ImGuiContext& g = *ImGui::GetCurrentContext();
    const ImVec2 hostMax(hostPos.x + hostSize.x, hostPos.y + hostSize.y);
    for (ImGuiWindow* w : g.Windows) {
        if (!isRegistered(m_names, w->Name)) {
            continue;
        }
        if (w->DockIsActive || w->Collapsed) {
            continue;
        }
        const float margin = 8.0f;
        const bool lostBelow = (w->Pos.y + w->Size.y < hostPos.y + margin);
        const bool lostRight = (w->Pos.x + w->Size.x < hostPos.x + margin);
        const bool lostLeft = (w->Pos.x > hostMax.x - margin);
        const bool lostTop = (w->Pos.y > hostMax.y - margin);
        if (!(lostBelow || lostRight || lostLeft || lostTop)) {
            continue;
        }
        w->Pos.x = std::clamp(w->Pos.x, hostPos.x, hostMax.x - 80.0f);
        w->Pos.y = std::clamp(w->Pos.y, hostPos.y, hostMax.y - 40.0f);
        w->Size.x = std::min(w->Size.x, hostSize.x);
        w->Size.y = std::min(w->Size.y, hostSize.y);
    }
}

// ------------------------------------------------------------------ focus --

std::string LayoutManager::focusedWindowName() const {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiWindow* win = g.NavWindow;
    if (!win && g.HoveredWindow) {
        win = g.HoveredWindow;
    }
    // Walk up to the top-level window so we always resolve to a *panel*
    // (Notes / Initiative / Projection), never a child window inside one.
    ImGuiWindow* top = win;
    if (top) {
        while (top->ParentWindow) {
            top = top->ParentWindow;
        }
        if (top && isRegistered(m_names, top->Name)) {
            return top->Name;
        }
    }
    // Fall back to the last remembered panel, then the first open one.
    if (isWindowOpen(m_lastFocused.c_str())) {
        return m_lastFocused;
    }
    for (const auto& n : m_names) {
        if (isWindowOpen(n.c_str())) {
            return n;
        }
    }
    return "";
}

void LayoutManager::dockFocusedTo(ImGuiID dockId, const ImVec2& hostSize,
                                  ImGuiDir dir, float ratio) {
    std::string name = focusedWindowName();
    if (name.empty()) {
        return;
    }
    rebuild(dockId, hostSize, name, dir, ratio);
}

void LayoutManager::maximize(ImGuiID dockId) {
    std::string focus = focusedWindowName();
    ImGui::DockBuilderRemoveNode(dockId);
    ImGuiID root =
        ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);

    // Dock others first so the focused window becomes the selected tab.
    for (const auto& n : m_names) {
        if (!isWindowOpen(n.c_str()) || n == focus) {
            continue;
        }
        ImGui::DockBuilderDockWindow(n.c_str(), root);
    }
    if (!focus.empty() && isWindowOpen(focus.c_str())) {
        ImGui::DockBuilderDockWindow(focus.c_str(), root);
    }
    ImGui::DockBuilderFinish(dockId);
    if (!focus.empty()) {
        ImGui::SetWindowFocus(focus.c_str());
    }
}

void LayoutManager::equalize(ImGuiID dockId) {
    ImGuiDockNode* root = ImGui::DockBuilderGetNode(dockId);
    if (!root || root->IsFloatingNode()) {
        return;
    }
    std::vector<ImGuiDockNode*> splits;
    std::vector<ImGuiDockNode*> stack{root};
    while (!stack.empty()) {
        ImGuiDockNode* node = stack.back();
        stack.pop_back();
        if (node->IsSplitNode()) {
            splits.push_back(node);
            if (node->ChildNodes[0]) {
                stack.push_back(node->ChildNodes[0]);
            }
            if (node->ChildNodes[1]) {
                stack.push_back(node->ChildNodes[1]);
            }
        }
    }
    for (ImGuiDockNode* n : splits) {
        if (!n->ChildNodes[0] || !n->ChildNodes[1]) {
            continue;
        }
        ImVec2 a, b;
        if (n->SplitAxis == ImGuiAxis_X) {
            a = ImVec2(n->Size.x * 0.5f, n->Size.y);
            b = ImVec2(n->Size.x * 0.5f, n->Size.y);
        } else {
            a = ImVec2(n->Size.x, n->Size.y * 0.5f);
            b = ImVec2(n->Size.x, n->Size.y * 0.5f);
        }
        ImGui::DockBuilderSetNodeSize(n->ChildNodes[0]->ID, a);
        ImGui::DockBuilderSetNodeSize(n->ChildNodes[1]->ID, b);
    }
}

void LayoutManager::focusPanelIndex(size_t index) {
    if (index >= m_names.size() || !isWindowOpen(m_names[index].c_str())) {
        return;
    }
    const std::string& name = m_names[index];
    m_lastFocused = name;
    // Drop widget-level focus so we land on the panel itself, not an input.
    ImGui::ClearActiveID();
    // Focus the window and make sure the dock tab is selected.
    if (ImGuiWindow* win = ImGui::FindWindowByName(name.c_str())) {
        ImGui::FocusWindow(win);
    }
    ImGui::SetWindowFocus(name.c_str());
}

void LayoutManager::focusDirectional(ImGuiID dockId, ImGuiDir dir) {
    // Collect leaf dock nodes that visibly hold a panel.
    ImGuiDockNode* root = ImGui::DockBuilderGetNode(dockId);
    if (!root) {
        return;
    }
    std::vector<ImGuiDockNode*> leaves;
    std::vector<ImGuiDockNode*> stack{root};
    while (!stack.empty()) {
        ImGuiDockNode* n = stack.back();
        stack.pop_back();
        if (n->IsSplitNode()) {
            if (n->ChildNodes[0]) {
                stack.push_back(n->ChildNodes[0]);
            }
            if (n->ChildNodes[1]) {
                stack.push_back(n->ChildNodes[1]);
            }
        } else if (n->VisibleWindow && isRegistered(m_names, n->VisibleWindow->Name)) {
            leaves.push_back(n);
        }
    }
    if (leaves.size() < 2) {
        return;
    }

    ImGuiDockNode* cur = nullptr;
    for (ImGuiDockNode* n : leaves) {
        if (n->VisibleWindow && m_lastFocused == n->VisibleWindow->Name) {
            cur = n;
            break;
        }
    }
    if (!cur) {
        if (std::string f = focusedWindowName(); !f.empty()) {
            for (ImGuiDockNode* n : leaves) {
                if (n->VisibleWindow && f == n->VisibleWindow->Name) {
                    cur = n;
                    break;
                }
            }
        }
    }
    if (!cur) {
        return;
    }

    const ImVec2 cc(cur->Pos.x + cur->Size.x * 0.5f,
                    cur->Pos.y + cur->Size.y * 0.5f);
    double best = 1e18;
    ImGuiDockNode* target = nullptr;
    for (ImGuiDockNode* n : leaves) {
        if (n == cur || !n->VisibleWindow) {
            continue;
        }
        const float nx = n->Pos.x + n->Size.x * 0.5f;
        const float ny = n->Pos.y + n->Size.y * 0.5f;
        const float dx = nx - cc.x;
        const float dy = ny - cc.y;
        bool ok = false;
        double cost = 0.0;
        switch (dir) {
            case ImGuiDir_Left:
                ok = dx < -1e-2f;
                cost = -dx + std::fabs(dy) * 0.01;
                break;
            case ImGuiDir_Right:
                ok = dx > 1e-2f;
                cost = dx + std::fabs(dy) * 0.01;
                break;
            case ImGuiDir_Up:
                ok = dy < -1e-2f;
                cost = -dy + std::fabs(dx) * 0.01;
                break;
            case ImGuiDir_Down:
                ok = dy > 1e-2f;
                cost = dy + std::fabs(dx) * 0.01;
                break;
            default:
                return;
        }
        if (ok && cost < best) {
            best = cost;
            target = n;
        }
    }
    if (target && target->VisibleWindow) {
        ImGui::SetWindowFocus(target->VisibleWindow->Name);
        m_lastFocused = target->VisibleWindow->Name;
    }
}

// ------------------------------------------------------------------ handle --

void LayoutManager::handleKeys(ImGuiID dockId, const ImVec2& hostSize) {
    (void)hostSize;
    ImGuiIO& io = ImGui::GetIO();

    // Remember which top-level panel currently has focus.
    {
        const std::string cur = focusedWindowName();
        if (!cur.empty()) {
            m_lastFocused = cur;
        }
    }

    // While a modal/popup is open (file dialogs, new-note, unsaved prompt),
    // do not touch the layout keys.
    const bool popupOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);

    // 1 / 2 / 3 select a top-level panel. These always work (even while a
    // text field is focused) so they are the reliable way to leave the
    // editor and grab a panel.
    if (!popupOpen) {
        if (ImGui::IsKeyPressed(ImGuiKey_1, false)) {
            focusPanelIndex(0);
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_2, false)) {
            focusPanelIndex(1);
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_3, false)) {
            focusPanelIndex(2);
            return;
        }
    }

    // The movement/layout keys only apply when a top-level window is focused
    // (a registered panel) and no text field is being typed in.
    const bool panelFocused = !m_lastFocused.empty() &&
                              isWindowOpen(m_lastFocused.c_str());
    if (popupOpen || io.WantTextInput || !panelFocused) {
        return;
    }

    auto queue = [&](PendingOp op, ImGuiDir dir = ImGuiDir_None,
                     float ratio = 0.5f) {
        m_pending = Pending{op, dir, ratio};
        m_pendingSet = true;
    };

    // Tab / Shift+Tab are deliberately NOT intercepted here: they keep
    // ImGui's built-in widget navigation. The layout keys below are the
    // movement-oriented ones only.
    // Arrow keys move focus between the panels.
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) {
        focusDirectional(dockId, ImGuiDir_Left);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) {
        focusDirectional(dockId, ImGuiDir_Right);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
        focusDirectional(dockId, ImGuiDir_Up);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
        focusDirectional(dockId, ImGuiDir_Down);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Equal, false)) {
        queue(PendingOp_Equalize);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_H, false)) {
        queue(PendingOp_Move, ImGuiDir_Left, 0.5f);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_L, false)) {
        queue(PendingOp_Move, ImGuiDir_Right, 0.5f);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_J, false)) {
        queue(PendingOp_Move, ImGuiDir_Down, 0.5f);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_K, false)) {
        queue(PendingOp_Move, ImGuiDir_Up, 0.5f);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        queue(PendingOp_Move, ImGuiDir_Right, 0.5f);  // vsplit
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_O, false)) {
        queue(PendingOp_Maximize);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_X, false)) {
        std::string name = focusedWindowName();
        if (!name.empty()) {
            setWindowOpen(name.c_str(), false);
        }
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        queue(PendingOp_Default);
        return;
    }
}

void LayoutManager::renderOverlay() {
    // Only show the keymap hint while a top-level panel is focused and no
    // text field is being typed in (i.e., when the shortcuts are active).
    if (ImGui::GetIO().WantTextInput || m_lastFocused.empty() ||
        !isWindowOpen(m_lastFocused.c_str())) {
        return;
    }
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 10.0f,
                                   vp->WorkPos.y + vp->WorkSize.y - 10.0f),
                            ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.8f);
    ImGui::Begin("##LayoutKeymap", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking |
                     ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("1/2/3        select panel");
    ImGui::Text("arrows       focus panel in direction");
    ImGui::Text("h/j/k/l      move panel to edge");
    ImGui::Text("s / S        split right / down");
    ImGui::Text("o            maximize panel");
    ImGui::Text("x            close panel (r reopens)");
    ImGui::Text("r            reset layout");
    ImGui::Text("=            equalize panes");
    ImGui::TextUnformatted("(Tab = default widget navigation)");
    ImGui::End();
}

}  // namespace ab