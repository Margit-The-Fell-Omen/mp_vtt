# Code style

The project does not ship a `.clang-format` file. Style is maintained by
convention; the rules below describe the existing code so new code stays
consistent. When in doubt, match the surrounding file.

## Language and standard

- C++17 for application code, C11 for the small C parts.
- Prefer standard library facilities (`std::filesystem`, `std::string`,
  `std::vector`, `std::shared_ptr`, `<algorithm>`).
- No exceptions in application code. Errors are reported with `bool`, null
  pointers, or `std::error_code`.
- No third-party abstraction layer is added when a small direct call will do.

## Formatting

- 4 spaces per indentation level; no tabs.
- Opening brace on the same line for functions, classes, namespaces, and
  control flow.
- `else` / `else if` on the same line as the closing brace.
- Keep lines close to 80 columns; slightly longer lines are acceptable for
  string literals and signatures.
- One statement per line.
- Headers use `#pragma once`.
- Include order inside a `.cpp`:
  1. the file's own header,
  2. other project headers (`app/...`, `core/...`, ...),
  3. third-party headers (`<glad/glad.h>`, `<GLFW/glfw3.h>`, `<imgui.h>`,
     `<sqlite3.h>`, `<stb_image.h>`),
  4. the C++ standard library.

Example:

```cpp
#include "projection/ProjectionWindow.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdio>
```

## Naming

| Kind | Convention | Examples |
| --- | --- | --- |
| Namespaces | lowercase | `ab` |
| Types / classes / structs | PascalCase | `Application`, `ProjectionWindow`, `Participant` |
| Functions / methods | PascalCase | `applyTheme()`, `showImage()`, `ensureSchema()` |
| Member variables | `m_` + camelCase | `m_window`, `m_selectedPath`, `m_projectionMonitor` |
| Local variables / parameters | camelCase | `monitorIndex`, `logicalW`, `quarterTurns` |
| Constants / schema strings | `k` + PascalCase | `kVertShader`, `kSchemaParticipant`, `kMaxHintQuads` |
| Enum values | `EnumName_Value` | `PendingOp_Default`, `PendingOp_Move` |
| Environment variables | `AB_` prefix | `AB_FONT`, `AB_GLFW_PLATFORM` |

Files are named after the primary type they contain (`Application.cpp`,
`NoteManager.h`). Header guards are not used because all headers are
`#pragma once`.

## Namespaces

- Public application code lives in `namespace ab`.
- File-local helpers live in an anonymous namespace at the top of the `.cpp`,
  or are `static` free functions.
- Do not use `using namespace` in headers. Avoid it in `.cpp` files.
- `namespace fs = std::filesystem;` is declared once in a header/namespace and
  reused (e.g. `ab::fs`).

## Headers

- Include only what the header needs; forward-declare when possible
  (`class Database;`, `struct ImVec4;`).
- Public classes keep data private and expose narrow accessors.
- Trivial accessors are defined inline in the header (`isOpen()`,
  `hasImage()`, `round()`).
- Prefer value members for owned data and `std::shared_ptr` only for genuinely
  shared resources (`Image`).

## Functions and control flow

- Guard clauses over deep nesting:

```cpp
if (!m_window || !m_shaderReady) {
    return;
}
```

- Use `nullptr`, not `0`/`NULL`.
- Mark overrides with `override`.
- Use `static_cast` for numeric conversions; avoid C-style casts except for
  documented third-party APIs (e.g. `reinterpret_cast<const char*>` when
  reading SQLite text).
- Prefer `const` references for non-trivial parameters and return values.
- Early-return on invalid indices/ranges rather than asserting.
- Small helpers are preferred over repeated inline logic; see
  `transformUV()`, `buildQuad()`, `runLen()`.

## Ownership and lifetimes

- `Application` owns the long-lived services by value.
- Resources wrapped in RAII where practical (`Database` closes in its
  destructor).
- GL resources are raw unsigned int IDs with explicit create/delete in
  `open()`/`close()` because they belong to a specific context.
- Images are `std::shared_ptr<Image>` so the gallery and projection can share
  pixels without copying.

## ImGui conventions

- Widgets are created directly in `Application::draw*` methods; state lives in
  `Application` members.
- Use stable widget IDs: `"##name"` for hidden labels, `PushID(index)` inside
  loops.
- Use `ImGui::BeginChild` with an explicit size for scrollable galleries and
  editor/preview panes.
- Disable unavailable actions with `BeginDisabled()`/`EndDisabled()`.
- Modals follow the existing pattern: a bool flag on `Application`, an
  `OpenPopup` call while the flag is set, then `BeginPopupModal`.
- Colors come from the active theme (`ImGui::GetStyle().Colors[...]`,
  `ab::accentColor()`, `ab::warningColor()`); do not hardcode UI colors.
- Keyboard handling must not steal input from text fields: check
  `io.WantTextInput` and `ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup)`
  before acting on single-key shortcuts.

## Comments

- Comments explain intent, invariants, or non-obvious workarounds, not what the
  next line does.
- Prefer short `//` sentences above the code.
- Keep comments current when behavior changes; stale comments are treated as
  bugs.
- Non-obvious platform behavior is documented at the call site (for example the
  XWayland/Hyprland fullscreen-monitor workaround in `ProjectionWindow.cpp`).

## Error handling and logging

- User-facing failures are surfaced in the UI where possible
  (disabled buttons, modal prompts, hint text).
- Non-recoverable startup failures print a single diagnostic to `stderr` and
  return a non-zero exit code.
- Recoverable file-system and database failures are silent or retried later;
  avoid noisy logging in the render loop.

## Third-party code

- `third_party/` and fetched dependencies are not restyled.
- Warnings from third-party sources are kept out of the project output by
  compiling them as separate targets without `-Wall -Wextra`.
- Wrap necessary third-party pragmas at the include site
  (`#pragma GCC diagnostic push/pop`) rather than editing vendored headers.

## Build and tooling

- The project builds with `-Wall -Wextra`; keep the build warning-free for
  project sources.
- `compile_commands.json` at the repo root is generated for clangd by
  `scripts/gen_compile_commands.sh`.
- Prefer CMake targets and compile definitions over ad-hoc preprocessor logic.
  Optional features use `AB_HAVE_*` definitions set by `CMakeLists.txt`.

## Platform code

The cross-platform work ([CROSS_PLATFORM.md](CROSS_PLATFORM.md)) established a
`src/platform/` boundary, and the Android phase will extend it:

- File access goes through `core/Storage.h`. `DesktopStorage` may use
  `std::filesystem`; Android will use SAF tree/document URIs.
- Do not include GLFW, EGL, X11, or Android headers from `core/`, `notes/`,
  `initiative/`, or `projection/`. Use `platform/GL.h`, `platform/Platform.h`,
  and `core/Storage.h` instead.
- Keep `#ifdef` blocks at the platform boundary, not scattered through services
  or UI code.
- Convert between persisted/UI UTF-8 strings and filesystem paths with
  `platform::pathFromUtf8()` / `platform::pathToUtf8()`.
- Android Java/Kotlin code lives under `android/`; keep the JNI surface
  small and wrap it in a C++ class in `src/platform/android/`.
- Treat SAF locations as opaque strings. Do not assume a path supports
  `std::filesystem` on Android.

## Tests

There is currently no automated test suite. When adding one, put it in a
separate CMake target and keep it independent of GLFW/ImGui where possible so
it can run headlessly. Pure logic (initiative ordering, palette derivation,
path/tree handling) is the best starting point.
