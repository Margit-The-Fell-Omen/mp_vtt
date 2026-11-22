# Architecture

Adventure Board is a single-process, single-threaded C++17 desktop application.
It uses GLFW for windowing, OpenGL 3.3 for rendering, and Dear ImGui (docking
branch) for the entire UI. SQLite stores settings and initiative state; notes
and images live in user-selected folders on disk.

The design goal is a small, explicit codebase: one coordinator object owns the
long-lived services, services expose plain C++ APIs, and UI code is drawn
immediately from `Application`'s panel methods.

## Platform status and seams

Linux is the validated runtime platform. Windows build support is implemented
but not yet validated on hardware. Android builds a debug APK with a
NativeActivity/EGL host but is not runtime-validated yet. The
[CROSS_PLATFORM.md](CROSS_PLATFORM.md) plan keeps `Application` and the services
shared and moves platform concerns behind narrow interfaces.

Implemented seams:

- `src/core/Storage.h` defines backend-neutral file operations;
  `src/platform/desktop/DesktopStorage` implements them with `std::filesystem`.
  NoteManager, ImageStore, and MarkdownRenderer use the interface.
- `ProjectionRenderer` owns the image/hint shaders and GL objects for both
  desktop GL and Android GLES3; `ProjectionWindow` owns the desktop window,
  context, callbacks, and monitor hints.
- `src/platform/Platform.h` owns home/config directories, URL opening, UTF-8
  path conversion, Windows drive roots, and byte reads.
- `src/platform/GL.h` selects glad on desktop and GLES3 headers on Android.
- `Application` has `initCore()` / `initGraphics()` and `shutdownGraphics()` /
  `shutdownCore()` so core state can outlive a graphics context.
- Fonts under `assets/fonts/` are embedded into the binary at build time and
  loaded from memory; there is no runtime font lookup or fontconfig dependency.

Android host status:

- `src/platform/android/android_main.cpp` implements NativeActivity, EGL,
  ImGui Android input, soft keyboard/Unicode JNI, and app lifecycle.
- `ProjectionWindowAndroid.cpp` is a stub; `Presentation` projection is pending.
- Notes/images default to app-private `notes/` and `images/` directories.
- SAF folder picking and full device validation are still pending.

References to GLFW, X11, and `~/.adventureboard` below describe the current
desktop implementation.

## High-level overview

```text
main()
  -> ab::Application::run()
       init()      open DB, create GLFW/OpenGL/ImGui, load settings & data
       loop        newFrame() -> draw() -> render main UI -> render projection
       shutdown()  persist state, destroy GL resources and windows
```

Services owned by `Application`:

```text
Application
  Settings             persisted key/value settings
  Database             SQLite RAII wrapper (settings + initiative tables)
  Project              current notes root folder
  NoteManager          notes tree + note file I/O
  ImageStore           image folder scan + image decoding
  ProjectionWindow     separate fullscreen GLFW window and GL context
  InitiativeTracker    combat participants, rounds, persistence
  LayoutManager        dock-tree layout, panel visibility, layout keys
```

Supporting modules:

```text
notes/MarkdownRenderer  ImGui markdown rendering (imgui_md + MD4C)
app/Theme               color palettes, built-in themes, font loading
initiative/Participant  plain data struct for one combatant
```

`main.cpp` only constructs `Application` and calls `run()`.

## Directory layout

```text
src/
  main.cpp
  app/
    Application.{h,cpp}    lifecycle, main menu, panels, modals, keyboard
    LayoutManager.{h,cpp}  dockspace default layout, bounds, layout hotkeys
    Theme.{h,cpp}          palette registry, theme application, fonts
  core/
    Database.{h,cpp}       SQLite connection + schema + query helpers
    Settings.{h,cpp}       persisted settings
    Project.h              notes root path
    Storage.h              backend-neutral file operations
  notes/
    NoteManager.{h,cpp}    folder scan, read/write/create/rename/remove
    MarkdownRenderer.{h,cpp} markdown -> ImGui, inline image cache
  initiative/
    InitiativeTracker.{h,cpp} encounter model + persistence
    Participant.h             participant data
  platform/
    Platform.{h,cpp}       config dir, URL open, path conversion, roots
    GL.h                   glad (desktop) / GLES3 (Android) includes
    desktop/
      DesktopStorage.{h,cpp} std::filesystem implementation
    android/
      android_main.cpp     NativeActivity + EGL + soft keyboard host
      ProjectionWindowAndroid.cpp  projection stub (Presentation pending)
  projection/
    ImageStore.{h,cpp}     image folder scan + stb_image decoding
    ProjectionRenderer.{h,cpp} shared image quad, transforms, hint text
    ProjectionWindow.{h,cpp} separate fullscreen GLFW window, callbacks,
                              monitor targeting

assets/fonts/  Liberation Sans/Mono + license (embedded at build)
third_party/
  glad/        vendored OpenGL loader
  imgui_md/    vendored ImGui markdown renderer (MD4C-backed)
```

The Android Gradle project lives in `android/` and builds the shared library
with the NDK. SAF storage and Presentation projection are still pending.

## Application lifecycle

`Application::init()` calls `initCore()` and then `initGraphics()`, so the
database and services can be created without a graphics context.

`initCore()`:

1. Create the config directory and open the SQLite database. On Android this is
   the app's `internalDataPath`, set by the NativeActivity host.
2. Ensure the schema and load persisted `Settings`.
3. Restore the notes/images roots and wire the services to `DesktopStorage`.
4. Rescan the notes/images folders and load the initiative encounter.
5. Register the dockable panels with `LayoutManager`.
6. Set the default new-note name.

`initGraphics()`:

1. Select the GLFW platform on Linux: X11 by default, Wayland via
   `AB_GLFW_PLATFORM=wayland`; fall back to GLFW's automatic choice.
2. Create the main window (OpenGL 3.3 core), load glad, create the ImGui
   context, apply the saved theme, load the embedded fonts, and enable docking
   + keyboard nav.
3. Point ImGui at `~/.adventureboard/imgui.ini`.
4. Initialize the GLFW/OpenGL ImGui backends.
5. Pick a default projection monitor (a monitor other than the app window's).

The frame loop in `newFrame()`:

1. `glfwPollEvents()`.
2. Restore the main window's GL context.
3. Close the projection if it received an Esc/click/close request.
4. ImGui new-frame, `draw()`.
5. `ImGui::Render()`, clear, render main UI, swap buffers.
6. Throttled window-size autosave.
7. `ProjectionWindow::render()` using the projection's own context.

`shutdown()` saves window/settings/initiative state, then calls
`shutdownGraphics()` (restore the main context, close the projection, shut down
ImGui backends and GLFW) and `shutdownCore()` (close the database).

On Android, `android_main.cpp` calls `initAndroid()` on `APP_CMD_INIT_WINDOW`,
`frameAndroid()` for each frame, and `shutdownAndroid()` on
`APP_CMD_TERM_WINDOW`. Core state survives surface recreation. App-private
`notes/` and `images/` directories are created on first launch until SAF
storage lands.

## UI architecture

- `draw()` creates a borderless host window over the viewport work area and a
  `DockSpace` (`MainDockSpace`) with passthrough central node.
- On first launch (no `imgui.ini`) `LayoutManager` builds the default layout:
  Notes and Initiative on the left, Projection on the right.
- Panels are drawn only if their layout window is open:
  `drawNotesPanel()`, `drawProjectionPanel()`, `drawInitiativePanel()`.
- Modals (folder picker, new note, theme picker, rename, delete,
  unsaved-changes prompt) are drawn from `draw()` after the panels.
- State for all UI dialogs is held as `Application` members; there is no
  separate view-model layer.

### LayoutManager

`LayoutManager` owns:

- the registered panel names and closed set (`x` closes, `r` reopens),
- keyboard layout commands (queued and applied at frame start),
- dock-tree rebuilding through `ImGui::DockBuilder*`,
- bounds repair for floating/undocked windows,
- the keyboard hint overlay.

Layout commands are queued in `handleKeys()` and executed by `applyPending()`
before panels are drawn, so dock changes take effect in the same frame.

## Notes subsystem

`NoteManager` scans the notes root recursively through `Storage` (skipping
permission errors on desktop) and stores a flat, sorted list of directories and
`.md` files. Paths are stored relative to the root; opaque absolute locations
are derived through `Storage::join()` on demand.

`Application` keeps:

- `m_selectedPath`, `m_editorBuffer`, `m_editorDirty`,
- `m_notesEditorOpen` / `m_notesPreviewOpen` for the Type/Render toggles,
- selection/cursor offsets used by the formatting buttons.

Editing uses `ImGui::InputTextMultiline`. Formatting buttons wrap the current
selection (or word under cursor) and re-sync ImGui's internal input state with
a pending selection. Saving writes through `NoteManager::write`.

Preview uses `MarkdownRenderer::render()`, which is a thin wrapper around the
vendored `imgui_md` renderer and MD4C. Relative images resolve against the
note's parent location through `Storage` and are uploaded once into a
process-wide GL texture cache keyed by the opaque location. External URLs open
through `platform::openUrl()`.

Folder handling: "Open folder..." and "Close folder" clear/load the project
root, tree, selection, and editor. The last closed path is remembered for quick
reopening. The persisted `last_project_path` is cleared when the folder is
closed, so a closed folder stays closed across restarts.

## Projection subsystem

`ImageStore` scans a flat image folder (`.png`, `.jpg`, `.jpeg`, `.bmp`,
`.tga`, `.gif`) through `Storage`, decodes with `stb_image` from an in-memory
byte buffer (vertical flip enabled so texture `v=0` is the image bottom), and
returns a `std::shared_ptr<Image>` holding RGBA8 pixels.

`ProjectionWindow` owns a **second GLFW window and OpenGL context**:

- fullscreen window on a chosen monitor,
- its own shader program, VAO/VBO, image texture, and a small text shader used
  for the "Press Esc or click to close projection" hint,
- `showImage()` / `clearImage()` and a blackout toggle,
- presentation state: quarter-turn rotation and horizontal/vertical mirroring.

`ProjectionRenderer` owns the shaders, VAOs/VBOs, texture, transform UV
math, and hint text. It has GLSL ES 3 variants for Android
(`#version 300 es`, `precision highp float`, no `core` profile), so the same
renderer can drive the desktop window and a future Android EGL surface.

Rendering uploads the texture when the image changes, builds an aspect-fit quad,
and applies rotation/mirroring as a UV transform. Transforms affect only the
currently projected image and reset when a different image is shown, the image
is cleared, or the projection closes. The quad's displayed width/height swap
for odd quarter turns so rotated images remain aspect-correct.

Important GL-context rule: the projection context is made current only while
its GL objects are touched, and the previously current context is restored
after `open()`, `close()`, and `render()`. The main UI frame must always render
with the main window's context current.

### Monitor targeting

GLFW's EWMH fullscreen-monitor hint is unreliable under XWayland when outputs
are scaled (multiple monitors can report position `0,0`). The projection
handles this in layers:

1. Create the fullscreen window on the selected `GLFWmonitor`.
2. Resolve the monitor to its Xinerama screen by logical size
   (physical size / content scale) and re-send
   `_NET_WM_FULLSCREEN_MONITORS` with the correct index for the first few
   frames after mapping.
3. On Hyprland (`HYPRLAND_INSTANCE_SIGNATURE` present), which ignores that hint
   for XWayland windows, move the focused projection window with
   `hyprctl dispatch movewindow mon:<name>`.

X11/Xinerama support is optional at build time (`AB_HAVE_X11`,
`AB_HAVE_XINERAMA`); without it the code falls back to GLFW behavior.

On Android there is no GLFW monitor list or second desktop window. The current
`ProjectionWindowAndroid.cpp` is a compile-able stub; the planned backend will
enumerate outputs through `DisplayManager` and render into a `Presentation`
`Surface` with EGL, reusing the shared projection renderer. Until then,
projection is desktop-only.

Input: the projection window has key and mouse-button callbacks. Esc or any
mouse press sets a close request, which `Application::newFrame()` consumes.
The main window's Esc key also closes an open projection.

## Initiative subsystem

`InitiativeTracker` is the encounter model:

- ordered `std::vector<Participant>` (name, initiative, HP/max HP, status),
- current index and round counter,
- sorting by initiative (descending) with name tie-break,
- next/previous/restart turn and HP clamping,
- save/load to SQLite tables `initiative_participants` and `initiative_meta`.

Persistence is a full replace of the participant rows on save. Participant IDs
are runtime-only; display order is persisted as `sort_order`. The panel marks
the tracker dirty on edits; `saveWindowState()` writes it if needed.

## Theme subsystem

`Theme.cpp` defines a `Palette` of ImGui color slots. Curated themes are
declared as a `ThemeDef` registry; most are generated from a small seed
(base/text/muted/accent/accent-text) by mixing surfaces, while the original
three palettes are explicit. `applyPalette()` writes `ImGuiStyle::Colors` and
shared rounding/padding metrics.

`availableThemes()` returns a stable dark-then-light list, which drives both
the theme picker and `]`/`[` cycling. `applyTheme()` also handles the three
built-in ImGui themes (`dark`, `classic`, `light`) and tracks whether the
current theme is light via `isLightTheme()`.

Theming is applied to the whole UI, including the input caret
(`ImGuiCol_InputTextCursor`), selected text, keyboard navigation cursor, and the
dockspace clear color. `accentColor()` and `warningColor()` expose mode-aware
colors for the few places that need semantic colors outside the style palette.

## Persistence and configuration

```text
~/.adventureboard/
  ab.db       SQLite database
  imgui.ini   ImGui docking/window layout
```

SQLite tables:

```text
settings                key/value pairs
  last_project_path     notes root (empty = closed)
  last_images_path      images root
  theme                 theme id
  window_width          main window width
  window_height         main window height

initiative_participants sort_order, name, initiative, hp, max_hp, status
initiative_meta         key/value (round, current)
```

Notes and images are not copied into the database; only their root paths are
remembered.

The Android host keeps the database and `imgui.ini` in
`activity->internalDataPath`. It currently uses app-private `notes/` and
`images/` directories; when SAF lands, its tree URIs will use the same
`last_project_path` / `last_images_path` settings keys. No schema migration is
required.

## Build system

- CMake >= 3.25, C++17, C11.
- `FetchContent` pulls GLFW 3.4, Dear ImGui (docking), MD4C 0.6.0, SQLite
  amalgamation, and stb.
- glad and imgui_md are vendored under `third_party/`.
- Bundled fonts live under `assets/fonts/` and are embedded at build time by
  `scripts/embed_fonts.py`; the executable needs no font files at runtime.
- Optional Linux integrations are compile-time flags:
  - `AB_HAVE_X11`, `AB_HAVE_XINERAMA` — projection monitor targeting.
- Our targets build with `-Wall -Wextra`; vendored code (imgui_md, stb,
  sqlite, glad) is not restyled and is built without the project warning flags.
- `scripts/gen_compile_commands.sh` generates a clangd-friendly
  `compile_commands.json` at the repo root after every configure/build. It is
  enabled only for desktop Unix builds.
- Self-contained executables: fonts are embedded at build time; Linux links
  libstdc++/libgcc statically; Windows uses the static MSVC runtime.
- Cross-platform build status:
  - Windows: MSVC + CMake with WGL and static CRT; X11 disabled; built by the
    GitHub Actions job (hardware validation pending).
  - Android: Gradle + CMake `externalNativeBuild`, NativeActivity + EGL +
    OpenGL ES 3, `native_app_glue`, and a shared-library target; debug APK
    builds locally with NDK 29.
  - CI: Linux CMake/Ninja, Windows MSVC, and the Android debug APK are
    configured in `.github/workflows/build.yml`.

## Threading and GL context rules

- The application is single-threaded. No background workers exist.
- Each GLFW window has its own OpenGL context. GL objects belong to the context
  that created them and must be created/destroyed with that context current.
- `ProjectionWindow` always restores the previous context on return.
- ImGui backend objects live in the main context and are initialized/shut down
  there.

## Error handling

- APIs return `bool` or null pointers; there are no exceptions in application
  code.
- Database errors are exposed through `Database::lastError()`.
- File-system operations use `std::error_code` where a failure is recoverable.
- Fatal initialization failures print to stderr and exit with a non-zero code.
- Unknown persisted theme ids fall back to the built-in dark theme.

## Extension guidelines

Adding a panel:

1. Add a `drawXPanel()` method and call it from `draw()`.
2. Register the panel name with `LayoutManager` in `init()`.
3. Add a View > Layout toggle if it should be user-toggleable.

Adding a theme:

1. Add a `ThemeDef` entry (or a seed passed to `makePalette()`) in
   `kPaletteThemes`.
2. Mark it light or dark; `availableThemes()` derives the picker/cycling order.
3. Keep every palette field populated.

Adding persisted state:

1. Add the field to `Settings` and load/save it in `Settings::load/save`
   using the existing `settings` table, or add a dedicated table with an
   idempotent `CREATE TABLE IF NOT EXISTS` schema like `InitiativeTracker`.
