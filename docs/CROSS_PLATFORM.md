# Cross-platform plan: Windows and Android

> Status: **Phase 1–2 implemented; Phase 3 in progress.** Linux is still the
> validated runtime platform. Windows build support and the shared Android
> seams are in place. Android now builds a signed debug APK with the NDK and
> has a NativeActivity/EGL host, soft keyboard, embedded bundled fonts, and
> app-private notes/images defaults. SAF storage, runtime validation, and
> Presentation projection are still pending.

## Goals

- Add **Windows** (MSVC + CMake) and **Android** (sideloaded APK) support.
- Keep Linux working exactly as it does today, including X11/XWayland monitor
  targeting and the opt-in Wayland path.
- Share the database schema, settings keys, note format, themes, initiative
  model, and projection transform logic across platforms.
- Keep platform-specific code at the edges instead of spreading `#ifdef`s
  through the services and UI.

## Non-goals

- Replacing GLFW on desktop. Android gets a separate NativeActivity host; the
  desktop window layer stays GLFW.
- Reworking the UI into a mobile-native design. Android uses the same ImGui
  panels, touch input, and scaled style.
- Play Store distribution. The first Android target is a sideloaded APK.

## Decisions

| Area | Decision |
| --- | --- |
| Platforms | Windows + Android; Linux preserved |
| Android feature scope | Full parity, including second-screen projection |
| Android storage | SAF user-picked folders (`ACTION_OPEN_DOCUMENT_TREE`) |
| Android shell | NativeActivity + EGL + OpenGL ES 3 + `imgui_impl_android` |
| Desktop shell | Existing GLFW + OpenGL 3.3 |
| Windows toolchain | MSVC + CMake |
| Android delivery | Sideloaded APK |
| Fonts | Embed an OFL font family into every executable/library |
| Desktop data directory | Keep `~/.adventureboard/` on Windows and Linux |
| Android data directory | App-private `activity->internalDataPath` (no home directory exists) |
| Folder picker | Keep the in-app ImGui picker; add Windows drive enumeration; use SAF on Android |
| Projection | Shared renderer; GLFW fullscreen window on desktop, `Presentation` + `Surface` + EGL on Android |

## Target architecture

```text
src/core/            Database, Settings, Project, Storage
src/notes/           NoteManager + MarkdownRenderer over Storage
src/initiative/      unchanged
src/projection/      ProjectionRenderer + desktop ProjectionWindow
src/app/             Application with initCore/initGraphics lifecycle split
src/platform/
  Platform.h/.cpp    config dir, URL open, path conversion, roots, file reads
  GL.h               glad on desktop, GLES3 on Android
  desktop/           DesktopStorage (std::filesystem)
  android/           NativeActivity host, EGL, JNI bridge; SAF pending
android/             Gradle app, MainActivity.java, manifest
assets/fonts/        Liberation fonts + license (embedded at build)
```

Implemented so far:

- `core/Storage.h` with `DesktopStorage` and the notes/images/markdown
  integration.
- `platform/Platform` for home/config paths, URL opening, UTF-8 path
  conversion, Windows drive roots, and byte reads.
- `platform/GL.h` for desktop GL versus Android GLES3 includes.
- A shared `ProjectionRenderer` (desktop GL and GLES3 shaders) used by
  `ProjectionWindow`.
- Lifecycle split into `initCore()` / `initGraphics()` and
  `shutdownGraphics()` / `shutdownCore()`.
- Embedded Liberation Sans/Mono fonts used on every platform.
- CMake guards for Windows and Linux-only integrations, plus Linux/Windows
  GitHub Actions build jobs.
- Android Gradle project, NativeActivity/EGL host, soft keyboard/Unicode JNI
  bridge, Android GLES3 renderer path, and debug APK build.
- Android app-private database, `imgui.ini`, and default notes/images folders.

`Application` must stop owning GLFW directly. It should keep the long-lived
services and expose lifecycle hooks that either host can drive.

## Phase plan

### Phase 0 — Baseline (done)

- Freeze or branch the current work in progress before starting refactors.
- Add the CI build matrix skeleton (Linux and Windows) before moving code.

Exit: a clean branch and a CI job that builds the current tree.

### Phase 1 — Windows desktop support (implemented, CI validation pending)

Target: about 1–2 days.

1. `CMakeLists.txt`
   - Find `Threads` before its first use.
   - Restrict X11 detection to `UNIX AND NOT ANDROID`.
   - Guard `regenerate_clangd_db` (bash + python) or make it optional.
   - Add Windows link requirements (`shell32`, `ole32`) and MSVC warning flags.
2. `src/app/Application.cpp`
   - Apply the X11/Wayland `glfwInitHint` only on Linux; Windows must use WGL.
   - Build config paths with `std::filesystem` joins.
3. `src/platform/Platform.{h,cpp}` (new)
   - `openUrl()` using `ShellExecuteW` on Windows and `xdg-open` on Linux.
   - `pathToUtf8()` / `pathFromUtf8()` using `u8string` / `u8path`.
   - `configDir()` with the same `~/.adventureboard` layout.
4. `src/notes/MarkdownRenderer.cpp`
   - Replace the direct `xdg-open` command with `platform::openUrl`.
5. Unicode paths
   - Read image files through `std::ifstream(fs::path)` and decode with
     `stbi_load_from_memory` instead of passing `path.string()` to `stbi_load`.
   - Use UTF-8 for persisted paths and UI labels.
6. Folder picker
   - Enumerate drives with `GetLogicalDriveStringsW` on Windows.
   - Add a “Location” text field for jumping to a path.
7. Fonts
   - Embed the bundled fonts into the executable; no runtime font lookup.

Exit: MSVC CI passes; the app runs with notes, images, projection, and
persistence on Windows; Linux is unchanged.

### Phase 2 — Shared seams for Android (implemented)

Target: about 3–5 days. Desktop-only refactor with no intended behavior
changes. Storage, lifecycle, GL includes, fonts, and the shared
`ProjectionRenderer` are done.

1. `src/core/Storage.h` (new)
   - Operations: list, read, write, create file, create directory, rename,
     delete, exists.
   - Locations are opaque strings: filesystem paths on desktop,
     `content://` tree URIs on Android.
   - `DesktopStorage` wraps the current `std::filesystem` behavior.
2. `Application` lifecycle
   - Split `init()` into `initCore()`, `initGraphics(nativeWindow)`,
     `frame()`, `shutdownGraphics()`, and `shutdownCore()`.
   - Android can recreate graphics on `APP_CMD_INIT_WINDOW` /
     `APP_CMD_TERM_WINDOW` without dropping data.
3. `src/platform/GL.h` (new)
   - Desktop: `<glad/glad.h>`.
   - Android: `<GLES3/gl3.h>`.
   - Remove direct glad/GLFW includes from `ProjectionWindow.cpp` and
     `MarkdownRenderer.cpp`.
4. Projection split
   - Extract shaders, quad building, texture upload, transforms, and the hint
     text into `ProjectionRenderer`.
   - Keep X11/Xinerama/Hyprland code in the desktop backend.
   - Add GLSL ES 3 shader variants (`#version 300 es`, `precision highp float`,
     no `core` profile).
5. Fonts
   - Embed the OFL family and its bold/italic variants plus a monospace face
     at build time.
   - Load through `AddFontFromMemoryTTF` with `FontDataOwnedByAtlas = false`.

Exit: Windows and Linux still build and behave identically; the desktop backend
uses the new seams.

### Phase 3 — Android shell: notes, initiative, SAF (in progress)

Target: about 1–2 weeks. Native host, soft keyboard, app-private storage, and
APK packaging are done; SAF storage and device validation are pending.

1. Build integration
   - Gradle project with `externalNativeBuild.cmake`.
   - Build the application as a `SHARED` library and link
     `android`, `EGL`, `GLESv3`, and `log`.
   - Add `android_native_app_glue.c` and
     `-u ANativeActivity_onCreate`.
   - Define `IMGUI_IMPL_OPENGL_ES3` and initialize the renderer with
     `#version 300 es`.
   - `minSdk` 26, target the current stable SDK, ABIs `arm64-v8a` and
     `x86_64`.
2. `src/platform/android/android_main.cpp` (done)
   - `android_main`, `ALooper` event loop, and lifecycle commands.
   - EGL context/surface setup following ImGui’s Android example.
   - `ImGui_ImplAndroid_Init` / `NewFrame`; input through
     `ImGui_ImplAndroid_HandleInputEvent`.
   - Soft keyboard via JNI (`showSoftInput` / `hideSoftInput`) when
     `io.WantTextInput` changes; poll Unicode characters through JNI.
   - Touch-sized scaling with `style.ScaleAllSizes()` and matching font sizes.
3. SAF storage (pending; app-private defaults are in place)
   - Java `MainActivity`: launch `ACTION_OPEN_DOCUMENT_TREE`, request
     read/write permission, and persist the grant with
     `takePersistableUriPermission`.
   - Native `AndroidSafStorage`: traverse with `DocumentsContract` child
     queries, read/write through `ContentResolver`, and create/rename/delete
     through `DocumentsContract`.
   - Cache relative paths to document URIs between rescans.
   - Persist tree URIs in the existing `last_project_path` and
     `last_images_path` settings keys; no schema migration.
   - Re-validate persisted URIs after restart.
4. Android UI behavior
   - “Open notes folder” and “Open images folder” launch SAF instead of the
     ImGui folder modal.
   - Database, settings, and `imgui.ini` live in `activity->internalDataPath`.

Exit: APK installs; SAF folders persist across restarts; note tree, create,
rename, delete, editing/saving, inline images, initiative, rotation, resume, and
the soft keyboard all work.

### Phase 4 — Android second-screen projection

Target: about 1–2 weeks. Highest-risk phase.

1. Android `Presentation` (Java/Kotlin)
   - Enumerate outputs with `DisplayManager.getDisplays(
     DISPLAY_CATEGORY_PRESENTATION)`.
   - Show a `Presentation` containing a `SurfaceView`.
   - Pass the `Surface` to native code through JNI.
2. Native projection backend
   - Wrap the surface with `ANativeWindow_fromSurface` and create an EGL
     window surface/context.
   - Reuse `ProjectionRenderer` for texture upload, aspect-fit, rotation,
     mirroring, blackout, and the hint text.
   - Replace desktop monitor enumeration with a shared display list.
3. UX
   - Show available external displays or “No external display”.
   - Back/touch closes the projection.
   - Transforms remain session-only.

Exit: the image projects to a real second display; transforms and blackout work;
the main UI context survives; surface loss and recreation recover cleanly.

### Phase 5 — CI, packaging, and documentation

- Add a GitHub Actions matrix:
  - Linux: CMake + Ninja.
  - Windows: MSVC + CMake.
  - Android: JDK 17, Android SDK/NDK, Gradle `assembleDebug`, APK artifact.
- Update the README, architecture notes, and roadmap with the supported matrix,
  Android storage model, and build instructions.
- Document APK signing and sideloading.

## Platform behavior matrix

| Area | Linux | Windows | Android |
| --- | --- | --- | --- |
| Windowing | GLFW / X11 or Wayland | GLFW / WGL | NativeActivity + EGL |
| Graphics | OpenGL 3.3 core | OpenGL 3.3 core | OpenGL ES 3 |
| Projection | GLFW fullscreen window | GLFW fullscreen window | Presentation API + Surface |
| Notes/images | Filesystem paths | Filesystem paths | SAF tree URIs |
| App data | `~/.adventureboard` | `~/.adventureboard` | App internal storage |
| URL opening | `xdg-open` | `ShellExecuteW` | Android `Intent` (planned) |
| Fonts | Embedded | Embedded | Embedded |
| Folder picker | In-app picker | In-app picker + drives | SAF picker |
| Input | Keyboard + mouse | Keyboard + mouse | Touch + soft keyboard |

## Compatibility contracts

These must not change between platforms:

- SQLite schema and settings keys (`settings`, `initiative_participants`,
  `initiative_meta`).
- Markdown note format and relative-image behavior.
- Initiative ordering, HP clamping, and round semantics.
- Theme ids and persisted layout data.
- Projection transform semantics (session-only, reset on image switch).

## Risks and mitigations

| Risk | Mitigation |
| --- | --- |
| SAF traversal is slow on large trees | Query `DocumentsContract` directly, cache URIs, reuse results per rescan |
| GL context loss on rotation/background | Separate core state from graphics lifecycle; recreate graphics safely |
| Presentation API unavailable on some devices | Detect through `DisplayManager`; show a clear message |
| JNI surface/thread lifetime bugs | Keep UI and surface calls on the Android main thread; acquire/release `ANativeWindow` carefully |
| Linux regression during refactor | Land Phase 2 behind the desktop implementation first; smoke-test notes, images, and projection |
| Soft keyboard and Unicode gaps | Reuse ImGui’s Android JNI flow for show/hide and Unicode polling |
| MSVC or Android NDK build failures | Add Windows and Android CI as soon as each phase compiles |

## Estimated effort

About **4–6 weeks for one experienced developer**. SAF storage and Android
second-screen projection are the long poles. Phase 1 (Windows) is independent
and can ship first.
