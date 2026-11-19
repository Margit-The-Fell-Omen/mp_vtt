# Adventure Board

A desktop application for running tabletop RPG sessions: markdown notes, an
initiative tracker, and a fullscreen image projection window for a second
display. Built with C++17, Dear ImGui, GLFW, OpenGL, MD4C, and SQLite.

Requested by the game club "Place of Adventure".

## Status

Active development. The notes, projection, and initiative flows are usable.
See [docs/ROADMAP.md](docs/ROADMAP.md) for a detailed list of implemented
features, known gaps, and planned work.

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
- OpenGL 3.3 capable drivers.
- Linux/X11 development headers for the default platform:
  `libx11`, `libxcursor`, `libxinerama`, `libxrandr`, `libxi`, `libGL`.
- Optional: fontconfig (system font discovery), Wayland client libraries
  (only when running with `AB_GLFW_PLATFORM=wayland`).
- Network access during the first configure: CMake fetches GLFW 3.4, Dear ImGui
  (docking), MD4C 0.6.0, SQLite, and stb. glad and imgui_md are vendored.

On NixOS the repository ships a dev shell with all of the above:

```sh
nix develop
```

## Build and run

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/AdventureBoard
```

If `build/` already exists, `cmake --build build` is enough.

The project builds with `-Wall -Wextra`; vendored third-party sources are
built separately without those flags.

## Configuration and data

Everything the app persists lives in `~/.adventureboard/`:

```text
~/.adventureboard/
  ab.db       SQLite database (settings, initiative encounter)
  imgui.ini   ImGui docking/window layout
```

Notes and images are regular folders on disk; only their root paths are stored
in the database. Closing the notes folder clears the saved notes root.

Environment variables:

| Variable | Effect |
| --- | --- |
| `AB_GLFW_PLATFORM=wayland` | Try the native Wayland backend instead of X11/XWayland. |
| `AB_GLFW_PLATFORM=x11` | Force X11 (the default). |
| `AB_FONT=/path/to/font.ttf` | Use a specific font file for the whole UI. |

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
- [Code style](docs/CODE_STYLE.md) — formatting, naming, ownership, ImGui
  conventions, and build tooling.
- [Roadmap](docs/ROADMAP.md) — implemented features, known gaps, and planned
  work.

## Project structure

```text
src/
  main.cpp
  app/         Application, LayoutManager, Theme
  core/        Database, Settings, Project
  notes/       NoteManager, MarkdownRenderer
  initiative/  InitiativeTracker, Participant
  projection/  ImageStore, ProjectionWindow
third_party/   glad, imgui_md
scripts/       compile_commands.json generator for clangd
docs/          architecture, code style, roadmap
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for details.

## Known limitations

- The File menu shows `Ctrl+N` for a new note, but that shortcut is not wired
  yet.
- External links in markdown are ignored.
- Notes directories cannot be renamed or deleted from the UI.
- The projection transform is session-only and resets when switching images.
- Projection monitor targeting has a Hyprland-specific fallback; other
  compositors rely on the EWMH hint.
- No automated tests, CI, or packaging yet.

## Third-party components

- [GLFW](https://www.glfw.org/) 3.4
- [Dear ImGui](https://github.com/ocornut/imgui) (docking branch)
- [MD4C](https://github.com/mity/md4c) 0.6.0
- [SQLite](https://www.sqlite.org/) amalgamation
- [stb](https://github.com/nothings/stb) (`stb_image`, `stb_easy_font`)
- [glad](https://github.com/Dav1dde/glad) (vendored)
- [imgui_md](third_party/imgui_md) (vendored)
