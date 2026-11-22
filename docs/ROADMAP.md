# Roadmap

This document separates what is implemented today from what is proposed next.
"Planned" items are intentions, not commitments; priorities may change based on
what the game club needs at the table.

## Implemented

### Application shell

- C++17 CMake project built with GLFW + OpenGL 3.3 + Dear ImGui docking branch.
  Linux is validated; Windows build support is implemented but not hardware-
  validated; Android builds an APK but is not runtime-validated. See
  [CROSS_PLATFORM.md](CROSS_PLATFORM.md).
- Main window with a dockable layout: Notes, Initiative, Projection.
- Main menu: File (new note, open notes folder, quit) and View (theme,
  layout reset, panel visibility).
- Docking layout is persisted to `~/.adventureboard/imgui.ini`.
- Window size autosaves (throttled) and is restored on startup.
- Default layout is built on first launch; layout can be reset at runtime.
- X11 is preferred by default on Linux with an opt-in Wayland path via
  `AB_GLFW_PLATFORM=wayland`.
- Liberation Sans/Mono fonts embedded into every executable/library at build
  time; no runtime font files or fontconfig dependency.

### Notes

- Open and close a notes root folder; the tree is rescanned recursively for
  `.md` files and directories.
- Reopen convenience: the folder picker starts at the last-used notes folder
  after a folder is closed.
- Folder tree with selection, new note, rename, and delete (files only).
- `InputTextMultiline` editor with unsaved-changes tracking, save button, and
  a discard/open confirmation prompt when switching notes or quitting.
- Formatting buttons: bold, italic, underline, strikethrough, inline code.
  They wrap the selection (or word under the cursor) and restore the selection
  after the edit.
- Live markdown preview through the vendored `imgui_md` renderer and MD4C.
- Type / Render checkboxes hide and reopen the editor and preview independently;
  the editor keeps its buffer when hidden.
- Relative inline images in the preview, cached as GL textures by absolute path.
- External URLs open in the system browser (`xdg-open` on Linux).

### Projection

- Separate fullscreen GLFW window with its own OpenGL context.
- Choose the target monitor from the Projection panel; the choice shows the
  monitor index and name.
- Open and close the projection from the Projection panel.
- Esc or any mouse click on the projection window closes it; Esc from the main
  window also closes it.
- When no image is projected, the black screen shows
  "Press Esc or click to close projection".
- Open an images folder and pick an image from a filename gallery.
- Project / Clear the selected image; texture upload only when it changes.
- Blackout toggle (hides the image while the window stays up).
- Presentation controls for the currently projected image:
  - rotate 90 degrees left or right,
  - mirror horizontally or vertically (buttons highlight when active),
  - reset transform,
  - rotation indicator.
- Rotate/mirror are UV transforms: the source image and gallery entries are
  never modified. Transforms reset when a different image is projected, when
  the image is cleared, or when the projection closes.
- Aspect-fit quad with width/height swapped for odd quarter turns.
- Correct monitor targeting on scaled XWayland setups:
  - re-send `_NET_WM_FULLSCREEN_MONITORS` with the Xinerama index resolved by
    logical size,
  - Hyprland fallback that moves the focused window with
    `hyprctl dispatch movewindow mon:<name>`.
- GL context save/restore around open/close/render so the main UI frame always
  renders in the main context.

### Initiative

- Participant list with name, initiative, current/max HP, and free-text status.
- Add/remove participants, sort by initiative (highest first, name tie-break).
- Next/previous turn with wrap-around; round counter advances when the order
  wraps; restart round.
- HP adjustment with clamping to `[0, maxHp]`; max HP editing.
- Current participant highlighted in the table.
- Encounter persisted to SQLite (participant rows plus round/current metadata).

### Themes and visual polish

- 23 selectable themes: 20 curated palettes with light/dark variants where the
  family provides them, plus the three built-in ImGui themes.
  - Catppuccin Mocha / Latte,
  - Rose Pine / Rose Pine Dawn,
  - Gruvbox Dark / Light,
  - Dracula / Alucard,
  - Tokyo Night / Light,
  - One Dark / Light,
  - Solarized Dark / Light,
  - Everforest Dark / Light,
  - Kanagawa Wave / Lotus,
  - Nord, Monokai,
  - ImGui Dark, Classic, Light.
- Every theme is classified light or dark.
- Theme picker modal grouped into dark and light sections.
- `]` / `[` cycle to the next/previous theme; the cycle wraps.
- Theme persists across restarts (unknown ids fall back to dark).
- Theme colors apply to widgets, text, selection, keyboard navigation cursor,
  input caret, and the dockspace clear color.
- Mode-aware helpers (`accentColor()`, `warningColor()`) keep the few semantic
  colors readable in both light and dark themes.

### Keyboard and layout

- `1` / `2` / `3`: focus Notes / Initiative / Projection.
- Arrow keys: move focus between panels.
- `h` / `j` / `k` / `l`: move the focused panel to an edge.
- `s`: split right.
- `o`: maximize the focused panel.
- `x`: close the focused panel (`r` reopens everything).
- `r`: reset layout.
- `=`: equalize panes.
- `Tab`: ImGui widget navigation (not intercepted).
- `]` / `[`: next / previous theme.
- `Esc`: close projection (main window or projection window).
- Layout shortcuts are suspended while typing in a text field or while a popup
  is open.

### Persistence and infrastructure

- Backend-neutral `Storage` abstraction with a `DesktopStorage` backend.
- Platform helpers for config paths, URL opening, UTF-8 paths, and Windows
  drive roots.
- SQLite settings table and initiative tables.
- Notes/images roots, theme, and window size persisted.
- CMake FetchContent for GLFW, Dear ImGui, MD4C, SQLite, and stb.
- Vendored glad and imgui_md.
- Optional X11/Xinerama integration via `AB_HAVE_*` flags.
- Warning-clean project build with `-Wall -Wextra`.
- clangd-friendly `compile_commands.json` generation.

## Known gaps

These are current limitations, not necessarily the next tasks:

- Windows build support is implemented but has not been validated on Windows
  hardware.
- Android builds a debug APK but has not been runtime-validated; SAF folder
  access and Presentation projection are still pending. The plan is in
  [CROSS_PLATFORM.md](CROSS_PLATFORM.md).
- The File menu advertises `Ctrl+N` for a new note, but the shortcut is not
  wired yet.
- Notes can only rename/delete files, not directories.
- No search, backlinks, or wiki-link navigation in notes.
- No autosave for notes; unsaved changes are only handled by the save button
  and the switch/quit prompt.
- Markdown image texture cache is unbounded.
- Projection transforms are session-only and reset when switching images.
- Projection monitor targeting relies on EWMH and has a Hyprland-specific
  fallback; other compositors may still need attention.
- X11 is the default platform because native Wayland can block during vsync
  when the window is occluded on some tiling compositors.
- Initiative supports one encounter at a time; status is free text.
- No automated tests, CI, or packaging.

## Planned / proposed

### Cross-platform (planned)

The full design and acceptance criteria are in
[CROSS_PLATFORM.md](CROSS_PLATFORM.md). Linux behavior is preserved throughout.

1. **Windows desktop** (implemented, validation pending)
   - MSVC + CMake build, WGL windowing, and CI.
   - Platform helpers for config paths, URL opening, and UTF-8 path handling.
   - In-app folder picker with Windows drive enumeration.
   - Bundled fonts shared with the other platforms.
2. **Shared seams** (implemented)
   - `Storage` abstraction, split graphics/core lifecycle, GL abstraction,
     bundled fonts, and the shared `ProjectionRenderer` are in place.
   - Desktop implementation keeps the current `std::filesystem` and GLFW
     behavior.
3. **Android shell** (in progress)
   - NativeActivity + EGL + OpenGL ES 3 host with `imgui_impl_android`.
   - Touch input and soft keyboard via JNI.
   - App-private database, `imgui.ini`, and default notes/images folders;
     SAF folder selection is still pending.
   - `minSdk` 26; sideloaded `arm64-v8a` / `x86_64` debug APK builds.
4. **Android projection**
   - Android `Presentation` + `SurfaceView` + EGL, reusing
     `ProjectionRenderer`.
   - External-display enumeration and graceful fallback when none is available.
5. **CI and packaging**
   - GitHub Actions matrix for Linux, Windows, and Android.
   - APK build artifact and signing/sideloading notes.

### Near term

- Wire the advertised `Ctrl+N` shortcut and add a small, documented shortcut
  table in the UI.
- Projection fit modes: fit, fill, and stretch; optional zoom/pan.
- Remember the last projection monitor and the last projected image across
  restarts.
- Add `Show` / `Hide` controls for the projection hint text and a short
  fade-out after the window opens.
- Notes: autosave or periodic draft recovery; search across notes.
- Notes: directory rename/delete and safer tree operations.
- Markdown: handle `file://` and local attachment links.
- Initiative: visible conditions/status editor, drag-to-reorder, and undo for
  HP changes.
- Add a settings dialog for theme, font, projection defaults, and window
  behavior instead of editing the database or using environment variables.

### Medium term

- Multiple named encounters with a load/save selector.
- Note templates and a quick "new note from template" flow.
- Projection slideshow / image queue with next/previous hotkeys.
- Per-image transform persistence (for example for maps that are always rotated
  the same way).
- Image thumbnail cache for the projection gallery.
- Theme improvements: follow the system light/dark preference, import custom
  themes, per-theme font sizing.
- Native Wayland support without the XWayland monitor workarounds, once the
  occlusion/vsync behavior is resolved.

### Long term / infrastructure

- Headless unit tests for pure logic: initiative ordering and HP clamping,
  palette derivation, note tree scanning, settings round-trips.
- Linux packaging (AppImage, Flatpak, or a Nix package) beyond the existing
  Linux/Windows CI jobs and the Android APK artifact.
- Structured logging with a debug flag instead of ad-hoc `stderr` messages.
- Crash/error reporting that keeps user data safe.
- Localization and accessibility review (keyboard-only workflows, contrast
  checks for every theme).

## How to contribute a roadmap item

1. Open an issue or start a branch describing the behavior.
2. Add or update the relevant model/service first, then the UI.
3. Update this document and, if the architecture changes, `ARCHITECTURE.md`.
4. Keep the build warning-free and verify both a dark and a light theme.
