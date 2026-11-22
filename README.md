# Adventure Board

A desktop application for running tabletop RPG sessions: markdown notes, an
initiative tracker, and a fullscreen image projection window for a second
display. Built with C++17, Dear ImGui, GLFW, OpenGL, MD4C, and SQLite.

Requested by the game club "Place of Adventure".

## Status

Active development. The notes, projection, and initiative flows are usable.
See [docs/ROADMAP.md](docs/ROADMAP.md) for a detailed list of implemented
features, known gaps, and planned work.

## Platform support

Linux is the validated runtime platform. Windows build support is implemented
(MSVC/CMake, platform helpers, UTF-8 paths, drive-aware folder picker, bundled
fonts) and has a CI build job, but has not been validated on Windows hardware
yet.

Android now has a native NativeActivity/EGL host, soft-keyboard bridge, bundled
font extraction, and an app-private notes/images default. It builds a signed
debug APK with the NDK. Runtime validation on a device/emulator is still
pending, and SAF folder access plus Presentation-based projection are not
implemented yet.

The shared core uses a `Storage` abstraction and platform helpers so the same
notes, initiative, theme, and database code runs on every platform.

See [docs/CROSS_PLATFORM.md](docs/CROSS_PLATFORM.md) for the target
architecture, phases, and platform behavior matrix.

## Features

### Notes

- Browse a folder tree of `.md` notes.
- Editor and live markdown preview, independently toggleable (Type / Render).
- Formatting buttons for bold, italic, underline, strikethrough, and inline
  code; they wrap the current selection and restore it afterwards.
- New, rename, and delete notes; unsaved-changes prompt when switching or
  quitting.
- Open and close the notes folder; reopening starts from the last-used path.
- Relative inline images in the preview.
- Fenced code blocks (with an optional language tag) and inline code, rendered
  in a monospace font with preserved whitespace.
- External links render as links and open in the system browser (`xdg-open` on
  Linux, `ShellExecute` on Windows).
- Nested ordered and unordered lists.
- Pressing Enter on a list line continues the list: indentation is preserved,
  ordered numbers increment, and a new bullet or task checkbox is inserted.
  Pressing Enter on an empty list item ends the list.

### Projection

- Separate fullscreen window on a chosen monitor, with its own OpenGL context.
- Open an images folder and pick an image from the gallery.
- Project / Clear the current image and toggle a blackout overlay.
- Rotate the projected image 90 degrees left/right, mirror horizontally or
  vertically, and reset the transform. These controls affect only the currently
  projected image and never modify the source file.
- Esc or a mouse click closes the projection; with no image the black screen
  shows a close hint.
- Correct monitor targeting on scaled XWayland setups (EWMH Xinerama index plus
  a Hyprland fallback).

### Initiative

- Participant list with name, initiative, HP/max HP, and status.
- Sort by initiative, next/previous turn, round tracking, restart round.
- HP adjustment with clamping, persisted to SQLite.

### UI

- Dockable panels with a saved layout and keyboard-driven layout commands.
- 23 themes with light/dark variants where available, a theme picker, and
  `]` / `[` cycling.
- Theme-aware caret, selection, keyboard navigation cursor, and dockspace
  background.

## Requirements

- CMake 3.25 or newer, Ninja (recommended), a C++17 compiler.
- Python 3 (used at build time to embed the bundled fonts).
- OpenGL 3.3 capable drivers.
- Android builds: JDK 17, Gradle, Android SDK (platform 36, build-tools 36.1)
  and NDK 29.0.14206865.
- Linux/X11 development headers for the default platform:
  `libx11`, `libxcursor`, `libxinerama`, `libxrandr`, `libxi`, `libGL`.
- Optional: Wayland client libraries (only when running with
  `AB_GLFW_PLATFORM=wayland`).
- Network access during the first configure: CMake fetches GLFW 3.4, Dear ImGui
  (docking), MD4C 0.6.0, SQLite, and stb. glad and imgui_md are vendored.
- Liberation Sans/Mono fonts under `assets/fonts/` are embedded into the
  executable/library at build time, so no font files are needed at runtime.

On NixOS the repository ships a dev shell with all of the above:

```sh
nix develop
```

The requirements and commands in this section describe the validated Linux
desktop build. Windows is implemented in the build and platform code but is not
validated on hardware yet. Android builds an APK but is not runtime-validated.
See [docs/CROSS_PLATFORM.md](docs/CROSS_PLATFORM.md).

## Build and run

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/AdventureBoard
```

If `build/` already exists, `cmake --build build` is enough.

The project builds with `-Wall -Wextra`; vendored third-party sources are
built separately without those flags.

Windows (MSVC):

```sh
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Both Linux and Windows are built by `.github/workflows/build.yml`.

Android debug APK:

```sh
export ANDROID_HOME=/path/to/android-sdk
export ANDROID_NDK_HOME=/path/to/ndk/29.0.14206865
./android/gradlew -p android :app:assembleDebug
# android/app/build/outputs/apk/debug/app-debug.apk
```

On NixOS, `nix develop .#android` provides the NDK, JDK, and Gradle. Point
`ANDROID_HOME` at an SDK containing platform 36 and build-tools 36.1. The
AGP-downloaded `aapt2` is a generic Linux binary; if it cannot run, patch its
interpreter or pass a patched binary with
`-Pandroid.aapt2FromMavenOverride=/path/to/aapt2`.

The desktop executables are self-contained: fonts and the Liberation license
are embedded at build time, Windows builds use the static MSVC runtime (no
VC++ redistributable), and Linux links libstdc++/libgcc statically. Linux still
uses the system X11/OpenGL/Wayland libraries. The Android APK embeds the native
library and fonts; no asset files or post-install setup are required.

## Configuration and data

Everything the app persists lives in `~/.adventureboard/`:

```text
~/.adventureboard/
  ab.db       SQLite database (settings, initiative encounter)
  imgui.ini   ImGui docking/window layout
```

Notes and images are regular folders on disk; only their root paths are stored
in the database. Closing the notes folder clears the saved notes root.

The plan keeps the same `~/.adventureboard/` layout on Windows. Android has no
equivalent home directory, so the database and `imgui.ini` live in the app's
private internal storage. On first launch the app creates `notes/` and
`images/` there. User-selected SAF folders are not wired yet; see
[docs/CROSS_PLATFORM.md](docs/CROSS_PLATFORM.md).

Environment variables:

| Variable | Effect |
| --- | --- |
| `AB_GLFW_PLATFORM=wayland` | Try the native Wayland backend instead of X11/XWayland. |
| `AB_GLFW_PLATFORM=x11` | Force X11 (the default). |

## Keyboard shortcuts

Layout shortcuts are active when a panel is focused and no text field or popup
is capturing input.

| Keys | Action |
| --- | --- |
| `1` / `2` / `3` | Focus Notes / Initiative / Projection |
| Arrow keys | Focus the panel in that direction |
| `h` / `j` / `k` / `l` | Move the focused panel to an edge |
| `s` | Split right |
| `o` | Maximize the focused panel |
| `x` | Close the focused panel (`r` reopens) |
| `r` | Reset layout |
| `=` | Equalize panes |
| `Tab` | ImGui widget navigation (not intercepted) |
| `]` / `[` | Next / previous theme |
| `Esc` | Close the projection (main or projection window) |

On the projection window, Esc or any mouse click closes it.

## Documentation

- [Architecture](docs/ARCHITECTURE.md) — modules, data flow, persistence,
  projection pipeline, and build integration.
- [Cross-platform plan](docs/CROSS_PLATFORM.md) — Windows and Android target
  architecture, phases, decisions, and risks.
- [Code style](docs/CODE_STYLE.md) — formatting, naming, ownership, ImGui
  conventions, and build tooling.
- [Roadmap](docs/ROADMAP.md) — implemented features, known gaps, and planned
  work.

## Project structure

```text
src/
  main.cpp
  app/         Application, LayoutManager, Theme
  core/        Database, Settings, Project, Storage
  notes/       NoteManager, MarkdownRenderer
  initiative/  InitiativeTracker, Participant
  platform/    Platform helpers, GL abstraction, DesktopStorage
  platform/android/  NativeActivity host, soft keyboard, projection stub
  projection/  ImageStore, ProjectionRenderer, ProjectionWindow
android/       Gradle app, Java NativeActivity, manifest
assets/        bundled fonts and license (embedded at build time)
third_party/   glad, imgui_md
scripts/       compile_commands.json generator for clangd
.github/       Linux and Windows build workflows
docs/          architecture, cross-platform plan, code style, roadmap
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for details.

## Known limitations

- The File menu shows `Ctrl+N` for a new note, but that shortcut is not wired
  yet.
- Notes directories cannot be renamed or deleted from the UI.
- The projection transform is session-only and resets when switching images.
- Projection monitor targeting has a Hyprland-specific fallback; other
  compositors rely on the EWMH hint.
- Windows build support is implemented but not validated on Windows hardware
  yet.
- Android builds an APK but has not been runtime-validated. SAF folder access
  and second-screen projection are still pending; notes/images default to
  app-private storage. The plan is in
  [docs/CROSS_PLATFORM.md](docs/CROSS_PLATFORM.md).
- No automated tests yet; Linux, Windows, and Android build jobs exist.

## Third-party components

- [GLFW](https://www.glfw.org/) 3.4
- [Dear ImGui](https://github.com/ocornut/imgui) (docking branch)
- [MD4C](https://github.com/mity/md4c) 0.6.0
- [SQLite](https://www.sqlite.org/) amalgamation
- [stb](https://github.com/nothings/stb) (`stb_image`, `stb_easy_font`)
- [glad](https://github.com/Dav1dde/glad) (vendored)
- [imgui_md](third_party/imgui_md) (vendored)
- Liberation Sans/Mono (embedded at build time, SIL OFL; see
  `assets/fonts/LICENSE-Liberation.txt`)
