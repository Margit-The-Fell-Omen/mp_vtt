#pragma once

#include "core/Database.h"
#include "core/Project.h"
#include "core/Settings.h"
#include "initiative/InitiativeTracker.h"
#include "notes/NoteManager.h"
#include "projection/ImageStore.h"
#include "projection/ProjectionWindow.h"

#include <memory>
#include <string>

namespace ab {

namespace fs = std::filesystem;

class Application {
public:
    Application();
    ~Application();

    int run();

private:
    bool init();
    void shutdown();
    void newFrame();
    void draw();
    void drawMenuBar();
    void drawNotesPanel();
    void drawProjectionPanel();
    void drawInitiativePanel();

    void openNotesFolder(const fs::path& path);
    void openImagesFolder(const fs::path& path);
    void selectNote(const std::string& relativePath);
    void requestOpenNote(const std::string& relativePath);
    void createNewNote();
    void beginRename(const std::string& relativePath);
    void doRename();
    void beginDelete(const std::string& relativePath);
    void doDelete();
    void requestQuit();
    void applySettingsTheme();
    void saveSettingsNow();

    // Modal folder picker. Returns true when a folder was chosen (sets out).
    bool drawFolderPicker(const char* title, std::string& out);

    void saveWindowState();

    Settings m_settings;
    Database m_database;
    Project m_project;
    NoteManager m_notes;
    ImageStore m_images;
    ProjectionWindow m_projection;
    InitiativeTracker m_initiative;

    // Initiative panel input state.
    char m_initiativeName[256] = {0};
    bool m_initiativeDirty = false;

    std::string m_iniPath;   // imgui.ini path (must outlive the pointer)
    bool m_needsDefaultLayout = true;

    // Notes UI state.
    std::string m_selectedPath;
    std::string m_editorBuffer;
    std::string m_loadedPath;
    bool m_editorDirty = false;

    // Rename / delete modals.
    bool m_renameOpen = false;
    std::string m_renameTarget;
    char m_renameName[256] = {0};

    bool m_deleteOpen = false;
    std::string m_deleteTarget;

    // Unsaved-changes prompt.
    bool m_promptUnsaved = false;
    std::string m_pendingOpen;
    bool m_quitRequested = false;

    // Live-resize autosave.
    int m_savedW = -1;
    int m_savedH = -1;
    double m_lastSettingsSave = 0.0;

    // Folder picker state.
    bool m_pickerOpen = false;
    std::string m_pickerTitle;
    std::string m_pickerCurrent;
    int m_pickerTarget = 0;  // 0 = notes, 1 = images

    // New note dialog state.
    bool m_newNoteOpen = false;
    char m_newNoteName[256] = {0};

    // Projection UI state.
    int m_selectedImage = -1;
    int m_projectionMonitor = 0;
    std::shared_ptr<Image> m_showingImage;

    void* m_window = nullptr;  // GLFWwindow*
};

}  // namespace ab