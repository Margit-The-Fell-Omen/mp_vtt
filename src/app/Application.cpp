#include "app/Application.h"

#include "notes/MarkdownRenderer.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <random>

namespace ab {

namespace fs = std::filesystem;

static void glfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

static std::string homeDir() {
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return home;
    }
    const char* up = std::getenv("USERPROFILE");
    if (up && *up) {
        return up;
    }
    return ".";
}

static std::string configDir() {
    return homeDir() + "/.adventureboard";
}

static std::string defaultDbPath() {
    return configDir() + "/ab.db";
}

static std::string defaultIniPath() {
    return configDir() + "/imgui.ini";
}

Application::Application() = default;

Application::~Application() = default;

bool Application::init() {
    // Load settings from DB before creating the window.
    fs::create_directories(configDir());
    if (!m_database.open(defaultDbPath())) {
        std::fprintf(stderr, "Failed to open database: %s\n",
                     m_database.lastError().c_str());
        return false;
    }
    m_database.ensureSchema();
    m_settings.load(m_database);

    m_project.setRootPath(m_settings.lastProjectPath);

    glfwSetErrorCallback(glfwErrorCallback);

    // On Wayland, GLFW blocks in the frame-callback during vsync when the
    // window is occluded (e.g. switching desktops on a tiling compositor),
    // which the compositor then reports as "application not responding".
    // Prefer X11/XWayland by default; allow opting back into Wayland via
    // the AB_GLFW_PLATFORM environment variable ("wayland" or "x11").
    {
        const char* platform = std::getenv("AB_GLFW_PLATFORM");
        bool useWayland = (platform && std::strcmp(platform, "wayland") == 0);
        if (useWayland) {
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
        } else {
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
        }
    }
    if (!glfwInit()) {
        // If X11 was unavailable, retry with automatic platform selection.
        glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
        if (!glfwInit()) {
            return false;
        }
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(m_settings.windowWidth, m_settings.windowHeight,
                                "Adventure Board", nullptr, nullptr);
    if (!m_window) {
        std::fprintf(stderr, "Failed to create GLFW window.\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(static_cast<GLFWwindow*>(m_window));
    glfwSwapInterval(1);  // vsync

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::fprintf(stderr, "Failed to initialize OpenGL loader (glad).\n");
        glfwTerminate();
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable |
                      ImGuiConfigFlags_NavEnableKeyboard;
    m_iniPath = defaultIniPath();
    m_needsDefaultLayout = !fs::exists(m_iniPath);
    io.IniFilename = m_iniPath.c_str();

    // Scale style/font a little larger for readability.
    ImGui::GetStyle().ScaleAllSizes(1.0f);

    ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(m_window), true);
    ImGui_ImplOpenGL3_Init("#version 330");

    m_notes.rescan(m_project.rootPath());
    m_images.rescan(m_settings.lastImagesPath);
    m_initiative.load(m_database);

    // Default new-note name.
    std::snprintf(m_newNoteName, sizeof(m_newNoteName), "New Note.md");
    return true;
}

void Application::saveWindowState() {
    GLFWwindow* window = static_cast<GLFWwindow*>(m_window);
    int w = 0;
    int h = 0;
    glfwGetWindowSize(window, &w, &h);
    if (w > 0 && h > 0) {
        m_settings.windowWidth = w;
        m_settings.windowHeight = h;
    }
    m_settings.lastProjectPath = m_project.rootPath();
    m_settings.lastImagesPath = m_images.root().string();
    m_settings.save(m_database);
    if (m_initiativeDirty) {
        m_initiative.save(m_database);
    }
}

void Application::shutdown() {
    saveWindowState();

    m_projection.close();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_window) {
        glfwDestroyWindow(static_cast<GLFWwindow*>(m_window));
    }
    glfwTerminate();
}

void Application::openNotesFolder(const fs::path& path) {
    if (path.empty()) {
        return;
    }
    m_project.setRootPath(path.string());
    m_notes.rescan(path);
    m_selectedPath.clear();
    m_editorBuffer.clear();
}

void Application::openImagesFolder(const fs::path& path) {
    if (path.empty()) {
        return;
    }
    m_images.rescan(path);
    m_selectedImage = -1;
}

void Application::selectNote(const std::string& relativePath) {
    if (relativePath == m_selectedPath) {
        return;
    }
    m_selectedPath = relativePath;
    if (!m_notes.read(relativePath, m_editorBuffer)) {
        m_editorBuffer.clear();
    }
    m_editorDirty = false;
}

void Application::createNewNote() {
    if (!m_notes.hasRoot()) {
        // No folder yet; guide the user to pick one first.
        m_newNoteOpen = false;
        m_pickerTitle = "Choose notes folder";
        m_pickerTarget = 0;
        m_pickerCurrent = homeDir();
        m_pickerOpen = true;
        return;
    }
    std::string name = m_newNoteName;
    if (name.empty()) {
        name = "New Note.md";
    } else if (fs::path(name).extension() != ".md") {
        name += ".md";
    }
    if (m_notes.create(name)) {
        m_notes.rescan(m_notes.root());
        selectNote(name);
    }
}

// Recursively renders the note tree. Directories are collapsible nodes,
// files are clickable leaf entries.
static void drawTree(const fs::path& dir, NoteManager& notes,
                     const std::string& selected,
                     const std::function<void(const std::string&)>& onSelect) {
    std::vector<NoteManager::Entry> entries;
    for (const auto& e : notes.entries(dir)) {
        if (e.path.parent_path() == dir) {
            entries.push_back(e);
        }
    }

    for (const auto& e : entries) {
        std::string label = e.path.filename().string();
        std::string id = "##" + e.path.string();

        if (e.isDir) {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                       ImGuiTreeNodeFlags_SpanAvailWidth;
            if (ImGui::TreeNodeEx(id.c_str(), flags, "%s/", label.c_str())) {
                drawTree(e.path, notes, selected, onSelect);
                ImGui::TreePop();
            }
        } else {
            bool isSelected = (selected == e.path.string());
            if (ImGui::Selectable(label.c_str(), isSelected)) {
                onSelect(e.path.string());
            }
        }
    }
}

void Application::drawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New note", "Ctrl+N")) {
                m_newNoteOpen = true;
            }
            if (ImGui::MenuItem("Open notes folder...")) {
                m_pickerTitle = "Choose notes folder";
                m_pickerTarget = 0;
                m_pickerCurrent = m_project.rootPath().empty()
                                      ? homeDir()
                                      : m_project.rootPath();
                m_pickerOpen = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) {
                glfwSetWindowShouldClose(static_cast<GLFWwindow*>(m_window),
                                         GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void Application::drawNotesPanel() {
    ImGui::Begin("Notes");

    // Toolbar: folder + actions.
    ImGui::TextDisabled("Folder: %s",
                        m_project.rootPath().empty() ? "(none)"
                                                     : m_project.rootPath().c_str());
    if (ImGui::Button("Open folder...")) {
        m_pickerTitle = "Choose notes folder";
        m_pickerTarget = 0;
        m_pickerCurrent = m_project.rootPath().empty() ? homeDir()
                                                       : m_project.rootPath();
        m_pickerOpen = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("New note")) {
        m_newNoteOpen = true;
    }
    ImGui::SameLine();
    bool canSave = !m_selectedPath.empty();
    if (!canSave) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Save")) {
        if (m_notes.write(m_selectedPath, m_editorBuffer)) {
            m_editorDirty = false;
        }
    }
    if (!canSave) {
        ImGui::EndDisabled();
    }
    if (m_editorDirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "*");
    }

    ImGui::Separator();

    // Left: tree.
    ImGui::BeginChild("notes_tree", ImVec2(240, 0), ImGuiChildFlags_Borders);
    if (m_notes.hasRoot()) {
        drawTree(fs::path(), m_notes, m_selectedPath,
                 [this](const std::string& p) { selectNote(p); });
    } else {
        ImGui::TextDisabled("Open a notes folder to begin.");
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right: editor + preview.
    ImGui::BeginChild("notes_content", ImVec2(0, 0), ImGuiChildFlags_Borders);

    if (m_selectedPath.empty()) {
        ImGui::TextDisabled("Select or create a note.");
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    float availH = ImGui::GetContentRegionAvail().y;
    float editorH = availH * 0.5f;
    float previewH = availH - editorH;

    ImGui::BeginChild("editor", ImVec2(0, editorH), ImGuiChildFlags_Borders);
    size_t cap = m_editorBuffer.size() + 1024;
    if (cap < 4096) {
        cap = 4096;
    }
    m_editorBuffer.resize(cap);
    if (ImGui::InputTextMultiline("##src", m_editorBuffer.data(),
                                  m_editorBuffer.size(),
                                  ImVec2(-1, -1),
                                  ImGuiInputTextFlags_AllowTabInput)) {
        m_editorDirty = true;
    }
    m_editorBuffer.resize(std::strlen(m_editorBuffer.c_str()));
    ImGui::EndChild();

    ImGui::BeginChild("preview", ImVec2(0, previewH), ImGuiChildFlags_Borders);
    MarkdownRenderer::render(m_editorBuffer);
    ImGui::EndChild();

    ImGui::EndChild();
    ImGui::End();
}

void Application::drawProjectionPanel() {
    ImGui::Begin("Projection");

    // Images folder.
    ImGui::TextDisabled("Folder: %s",
                        m_images.root().empty() ? "(none)"
                                                : m_images.root().c_str());
    if (ImGui::Button("Open folder...")) {
        m_pickerTitle = "Choose images folder";
        m_pickerTarget = 1;
        m_pickerCurrent = m_images.root().empty() ? homeDir()
                                                  : m_images.root().string();
        m_pickerOpen = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Rescan")) {
        m_images.rescan(m_images.root());
        m_selectedImage = -1;
    }

    // Monitor + open/close + blackout.
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    ImGui::Text("Monitor:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo("##monitor", std::to_string(m_projectionMonitor).c_str())) {
        for (int i = 0; i < monitorCount; ++i) {
            std::string name = std::to_string(i);
            if (glfwGetMonitorName(monitors[i])) {
                name += " - ";
                name += glfwGetMonitorName(monitors[i]);
            }
            if (ImGui::Selectable(name.c_str(), i == m_projectionMonitor)) {
                m_projectionMonitor = i;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (m_projection.isOpen()) {
        if (ImGui::Button("Close projection")) {
            m_projection.close();
        }
    } else {
        if (ImGui::Button("Open projection")) {
            m_projection.open(m_projectionMonitor);
        }
    }

    ImGui::SameLine();
    bool blackout = m_projection.blackout();
    if (ImGui::Checkbox("Blackout", &blackout)) {
        m_projection.setBlackout(blackout);
    }

    ImGui::Separator();

    // Gallery.
    const auto& images = m_images.images();
    ImGui::BeginChild("gallery", ImVec2(0, -56), ImGuiChildFlags_Borders);
    if (images.empty()) {
        ImGui::TextDisabled("Open an images folder to browse.");
    }

    const float thumb = 120.0f;
    const float cell = thumb + 16.0f;
    float availW = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, static_cast<int>(availW / cell));

    int idx = 0;
    const int count = static_cast<int>(images.size());
    while (idx < count) {
        for (int c = 0; c < cols && idx < count; ++c, ++idx) {
            const fs::path& p = images[idx];
            std::string label = p.filename().string();

            ImGui::PushID(idx);
            bool sel = (idx == m_selectedImage);
            if (sel) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.5f, 0.85f, 1.0f));
            }
            if (ImGui::Button(label.c_str(), ImVec2(thumb, thumb))) {
                m_selectedImage = idx;
            }
            if (sel) {
                ImGui::PopStyleColor();
            }
            ImGui::PopID();

            if (c + 1 < cols && idx + 1 < count) {
                ImGui::SameLine();
            }
        }
    }
    ImGui::EndChild();

    // Selected image controls.
    if (m_selectedImage >= 0 && m_selectedImage < static_cast<int>(images.size())) {
        ImGui::TextUnformatted(images[m_selectedImage].filename().c_str());
        ImGui::SameLine();
        if (ImGui::Button("Project")) {
            m_showingImage = m_images.load(images[m_selectedImage]);
            m_projection.showImage(m_showingImage);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            m_showingImage.reset();
            m_projection.clearImage();
        }
    }

    ImGui::End();
}

void Application::drawInitiativePanel() {
    ImGui::Begin("Initiative");

    // Add row.
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("Name", m_initiativeName, sizeof(m_initiativeName));
    ImGui::SameLine();
    static int newInit = 0;
    ImGui::SetNextItemWidth(60);
    ImGui::InputInt("Init", &newInit);
    ImGui::SameLine();
    if (ImGui::Button("Add")) {
        std::string name = m_initiativeName[0] ? m_initiativeName : "Combatant";
        m_initiative.addParticipant(name);
        m_initiative.participants().back().initiative = newInit;
        m_initiativeName[0] = '\0';
        m_initiativeDirty = true;
    }

    ImGui::Separator();

    // Turn controls.
    if (ImGui::Button("Previous")) {
        m_initiative.previousTurn();
    }
    ImGui::SameLine();
    int round = m_initiative.round();
    ImGui::SetNextItemWidth(50);
    if (ImGui::InputInt("Round", &round)) {
        m_initiative.setRound(round);
        m_initiativeDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Next turn")) {
        m_initiative.nextTurn();
    }
    ImGui::SameLine();
    if (ImGui::Button("Sort by init")) {
        m_initiative.sortByInitiative();
        m_initiativeDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Roll all d20")) {
        static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> d20(1, 20);
        for (auto& p : m_initiative.participants()) {
            p.initiative = d20(rng);
        }
        m_initiative.sortByInitiative();
        m_initiativeDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear all")) {
        m_initiative.participants().clear();
        m_initiative.restartRound();
        m_initiative.setRound(1);
        m_initiativeDirty = true;
    }

    ImGui::Separator();

    auto& parts = m_initiative.participants();
    if (parts.empty()) {
        ImGui::TextDisabled("No combatants. Add one above.");
        ImGui::End();
        return;
    }

    static const ImGuiTableFlags flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("init_table", 6, flags)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Init", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("HP", ImGuiTableColumnFlags_WidthFixed, 200);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableHeadersRow();

        int removeId = -1;
        for (size_t i = 0; i < parts.size(); ++i) {
            Participant& p = parts[i];
            ImGui::PushID(p.id);
            ImGui::TableNextRow();

            if (m_initiative.isCurrent(i)) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                       ImGui::GetColorU32(ImVec4(0.2f, 0.45f, 0.8f, 0.55f)));
            }

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", static_cast<int>(i) + 1);

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1);
            char nameBuf[256];
            std::snprintf(nameBuf, sizeof(nameBuf), "%s", p.name.c_str());
            if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) {
                p.name = nameBuf;
                m_initiativeDirty = true;
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-1);
            int init = p.initiative;
            if (ImGui::InputInt("##init", &init)) {
                p.initiative = init;
                m_initiativeDirty = true;
            }

            ImGui::TableSetColumnIndex(3);
            float hpW = (ImGui::GetColumnWidth() - 12.0f) * 0.5f;
            if (hpW < 40.0f) {
                hpW = 40.0f;
            }
            ImGui::SetNextItemWidth(hpW);
            int hp = p.hp;
            if (ImGui::InputInt("##hp", &hp)) {
                m_initiative.adjustHp(static_cast<int>(i), hp - p.hp);
                m_initiativeDirty = true;
            }
            ImGui::SameLine(0, 2);
            ImGui::Text("/");
            ImGui::SameLine(0, 2);
            ImGui::SetNextItemWidth(hpW);
            int maxHp = p.maxHp;
            if (ImGui::InputInt("##maxhp", &maxHp)) {
                m_initiative.setMaxHp(static_cast<int>(i), maxHp);
                m_initiativeDirty = true;
            }

            ImGui::TableSetColumnIndex(4);
            ImGui::SetNextItemWidth(-1);
            char statusBuf[256];
            std::snprintf(statusBuf, sizeof(statusBuf), "%s", p.status.c_str());
            if (ImGui::InputText("##status", statusBuf, sizeof(statusBuf))) {
                p.status = statusBuf;
                m_initiativeDirty = true;
            }

            ImGui::TableSetColumnIndex(5);
            if (ImGui::SmallButton("X")) {
                removeId = p.id;
            }

            ImGui::PopID();
        }
        ImGui::EndTable();

        if (removeId >= 0) {
            m_initiative.removeParticipant(removeId);
            m_initiativeDirty = true;
        }
    }

    ImGui::End();
}

bool Application::drawFolderPicker(const char* title, std::string& out) {
    if (!m_pickerOpen) {
        return false;
    }
    bool chosen = false;

    ImGui::OpenPopup(title);
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(m_pickerCurrent.c_str());
        ImGui::Separator();

        fs::path cur(m_pickerCurrent);
        if (cur.has_parent_path() && cur != cur.root_path()) {
            if (ImGui::Selectable("  ..")) {
                m_pickerCurrent = cur.parent_path().string();
            }
        }

        std::vector<fs::path> dirs;
        std::error_code ec;
        for (auto it = fs::directory_iterator(cur, ec);
             it != fs::directory_iterator(); it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            bool isDir = it->is_directory(ec);
            if (ec) {
                ec.clear();
                continue;
            }
            if (isDir) {
                dirs.push_back(it->path());
            }
        }
        std::sort(dirs.begin(), dirs.end());

        if (ImGui::BeginChild("picker_dirs", ImVec2(420, 320), true)) {
            for (const auto& d : dirs) {
                std::string label = "  " + d.filename().string();
                if (ImGui::Selectable(label.c_str())) {
                    m_pickerCurrent = d.string();
                }
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        if (ImGui::Button("Select this folder")) {
            out = m_pickerCurrent;
            chosen = true;
            m_pickerOpen = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_pickerOpen = false;
        }

        ImGui::EndPopup();
    }

    return chosen;
}

void Application::draw() {
    ImGui::DockSpaceOverViewport(ImGui::GetID("MainDockSpace"),
                                 ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);

    // One-time default docking layout on first launch.
    if (m_needsDefaultLayout) {
        m_needsDefaultLayout = false;
        ImGuiID id = ImGui::GetID("MainDockSpace");
        ImGui::DockBuilderRemoveNode(id);
        ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(id, ImGui::GetMainViewport()->WorkSize);

        ImGuiID left;
        ImGuiID right;
        ImGui::DockBuilderSplitNode(id, ImGuiDir_Left, 0.64f, &left, &right);

        ImGuiID notes;
        ImGuiID initiative;
        ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.55f, &notes, &initiative);

        ImGui::DockBuilderDockWindow("Notes", notes);
        ImGui::DockBuilderDockWindow("Initiative", initiative);
        ImGui::DockBuilderDockWindow("Projection", right);
        ImGui::DockBuilderFinish(id);
    }

    drawMenuBar();
    drawNotesPanel();
    drawProjectionPanel();
    drawInitiativePanel();

    // Folder picker modal.
    std::string picked;
    if (m_pickerOpen) {
        if (drawFolderPicker(m_pickerTitle.c_str(), picked)) {
            if (m_pickerTarget == 0) {
                openNotesFolder(picked);
            } else {
                openImagesFolder(picked);
            }
        }
    }

    // New-note dialog.
    if (m_newNoteOpen) {
        ImGui::OpenPopup("New note");
        if (ImGui::BeginPopupModal("New note", &m_newNoteOpen,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("Filename", m_newNoteName, sizeof(m_newNoteName));
            if (ImGui::Button("Create")) {
                createNewNote();
                m_newNoteOpen = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                m_newNoteOpen = false;
            }
            ImGui::EndPopup();
        }
    }
}

void Application::newFrame() {
    glfwPollEvents();

    // Projection window (when open) leaves its own context current on the
    // previous frame; restore the main window's context for ImGui.
    glfwMakeContextCurrent(static_cast<GLFWwindow*>(m_window));

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    draw();

    ImGui::Render();

    GLFWwindow* window = static_cast<GLFWwindow*>(m_window);
    int displayW = 0;
    int displayH = 0;
    glfwGetFramebufferSize(window, &displayW, &displayH);
    glViewport(0, 0, displayW, displayH);
    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);

    // Render the projection window (its own context).
    m_projection.render();
}

int Application::run() {
    if (!init()) {
        return 1;
    }

    while (!glfwWindowShouldClose(static_cast<GLFWwindow*>(m_window))) {
        newFrame();
    }

    shutdown();
    return 0;
}

}  // namespace ab