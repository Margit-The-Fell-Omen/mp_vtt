cmake_minimum_required(VERSION 3.25)
project(AdventureBoard LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

find_package(Threads REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# GLFW (desktop windowing; Android uses NativeActivity)
# ---------------------------------------------------------------------------
if(NOT ANDROID)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  if(UNIX AND NOT APPLE)
    find_program(AB_WAYLAND_SCANNER wayland-scanner)
    if(NOT AB_WAYLAND_SCANNER)
      message(STATUS "wayland-scanner not found; disabling GLFW Wayland backend")
      set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    endif()
  endif()
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
  )
endif()

# ---------------------------------------------------------------------------
# glad (desktop OpenGL loader)
# ---------------------------------------------------------------------------
if(NOT ANDROID)
  add_library(glad STATIC
    third_party/glad/glad.c
  )
  target_include_directories(glad PUBLIC third_party/glad)
endif()

# ---------------------------------------------------------------------------
# Dear ImGui
# ---------------------------------------------------------------------------
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
  GIT_SHALLOW TRUE
)

# ---------------------------------------------------------------------------
# MD4C (Markdown parser)
# ---------------------------------------------------------------------------
FetchContent_Declare(
  md4c
  GIT_REPOSITORY https://github.com/mity/md4c.git
  GIT_TAG v0.6.0
  GIT_SHALLOW TRUE
)

# ---------------------------------------------------------------------------
# SQLite (amalgamation via CMake)
# ---------------------------------------------------------------------------
FetchContent_Declare(
  sqlite3
  URL https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sqlite3)

# ---------------------------------------------------------------------------
# stb (single-header image loading)
# ---------------------------------------------------------------------------
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(stb)

# Make remaining targets available
if(ANDROID)
  FetchContent_MakeAvailable(imgui md4c)
else()
  FetchContent_MakeAvailable(glfw imgui md4c)
endif()

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
set(AB_COMMON_SOURCES
  src/app/Application.cpp
  src/app/LayoutManager.cpp
  src/app/Theme.cpp
  src/core/Database.cpp
  src/core/Settings.cpp
  src/initiative/InitiativeTracker.cpp
  src/notes/NoteManager.cpp
  src/notes/MarkdownRenderer.cpp
  src/platform/Platform.cpp
  src/platform/desktop/DesktopStorage.cpp
  src/projection/ImageStore.cpp
  src/projection/ProjectionRenderer.cpp
)

if(ANDROID)
  add_library(AdventureBoard SHARED
    ${AB_COMMON_SOURCES}
    src/platform/android/ProjectionWindowAndroid.cpp
    src/platform/android/android_main.cpp
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
  )
else()
  add_executable(AdventureBoard
    ${AB_COMMON_SOURCES}
    src/main.cpp
    src/projection/ProjectionWindow.cpp
  )
endif()

# imgui_md is a vendored third-party library; build it without our
# -Wall -Wextra flags so upstream warnings stay out of the output.
add_library(imgui_md STATIC
  third_party/imgui_md/imgui_md.cpp
)
if(ANDROID)
  set_target_properties(imgui_md PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(imgui_md PUBLIC
  third_party/imgui_md
  ${imgui_SOURCE_DIR}
  ${md4c_SOURCE_DIR}/src
)

target_include_directories(AdventureBoard PRIVATE
  src
  third_party/imgui_md
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${md4c_SOURCE_DIR}/src
  ${sqlite3_SOURCE_DIR}
  ${stb_SOURCE_DIR}
)

if(ANDROID)
  target_include_directories(AdventureBoard PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue)
endif()

# ImGui sources
target_sources(AdventureBoard PRIVATE
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

if(ANDROID)
  target_sources(AdventureBoard PRIVATE
    ${imgui_SOURCE_DIR}/backends/imgui_impl_android.cpp)
else()
  target_sources(AdventureBoard PRIVATE
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)
endif()

# MD4C sources
target_sources(AdventureBoard PRIVATE
  ${md4c_SOURCE_DIR}/src/md4c.c
  ${md4c_SOURCE_DIR}/src/entity.c
)

# SQLite amalgamation source
target_sources(AdventureBoard PRIVATE
  ${sqlite3_SOURCE_DIR}/sqlite3.c
)

# Fontconfig is a Linux-only optional integration. Windows and Android use the
# bundled fonts instead.
if(UNIX AND NOT APPLE AND NOT ANDROID)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(FONTCONFIG QUIET fontconfig)
  endif()
endif()

if(ANDROID)
  target_link_libraries(AdventureBoard PRIVATE
    imgui_md
    Threads::Threads
    android
    EGL
    GLESv3
    log
  )
  target_link_options(AdventureBoard PRIVATE
    "-u" "ANativeActivity_onCreate")
else()
  target_link_libraries(AdventureBoard PRIVATE
    glfw
    glad
    imgui_md
    Threads::Threads
    ${CMAKE_DL_LIBS}
  )
  if(WIN32)
    target_link_libraries(AdventureBoard PRIVATE shell32 ole32)
  endif()
endif()

# X11 / Xinerama (projection fullscreen targeting on X11/XWayland).
# Under XWayland, GLFW's monitor->Xinerama index mapping can be wrong when the
# compositor scales outputs, so the projection window re-sends
# _NET_WM_FULLSCREEN_MONITORS with the correct index itself.
if(UNIX AND NOT APPLE AND NOT ANDROID)
  find_package(X11)
  if(X11_FOUND)
    target_include_directories(AdventureBoard PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(AdventureBoard PRIVATE ${X11_LIBRARIES})
    target_compile_definitions(AdventureBoard PRIVATE AB_HAVE_X11=1)
    if(X11_Xinerama_FOUND)
      target_include_directories(AdventureBoard PRIVATE
        ${X11_Xinerama_INCLUDE_PATH})
      target_link_libraries(AdventureBoard PRIVATE ${X11_Xinerama_LIB})
      target_compile_definitions(AdventureBoard PRIVATE AB_HAVE_XINERAMA=1)
    endif()
  endif()
endif()

if(FONTCONFIG_FOUND)
  target_include_directories(AdventureBoard PRIVATE ${FONTCONFIG_INCLUDE_DIRS})
  target_link_libraries(AdventureBoard PRIVATE ${FONTCONFIG_LIBRARIES})
  target_compile_definitions(AdventureBoard PRIVATE AB_HAVE_FONTCONFIG=1)
endif()

if(ANDROID)
  target_compile_definitions(AdventureBoard PRIVATE
    IMGUI_IMPL_OPENGL_ES3
    AB_ASSET_DIR=""
  )
else()
  target_compile_definitions(AdventureBoard PRIVATE
    GLFW_INCLUDE_NONE
    AB_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
  )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(AdventureBoard PRIVATE -Wall -Wextra)
  # stb/sqlite are C; keep warnings for our own C++ only
endif()

# Regenerate a clangd-friendly compile_commands.json at the repo root after
# every build/configure (see scripts/gen_compile_commands.sh). The script needs
# bash and python3, so keep it on desktop Unix builds only.
if(UNIX AND NOT APPLE AND NOT ANDROID)
  add_custom_target(regenerate_clangd_db ALL
    COMMAND ${CMAKE_COMMAND} -E env CXX=${CMAKE_CXX_COMPILER}
            bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_compile_commands.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Regenerating root compile_commands.json for clangd"
    VERBATIM
  )
endif()