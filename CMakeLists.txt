cmake_minimum_required(VERSION 3.25)
project(AdventureBoard LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# GLFW
# ---------------------------------------------------------------------------
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
  GIT_SHALLOW TRUE
)

# ---------------------------------------------------------------------------
# glad (OpenGL loader) - vendored pre-generated single-file loader
# ---------------------------------------------------------------------------
add_library(glad STATIC
  third_party/glad/glad.c
)
target_include_directories(glad PUBLIC third_party/glad)

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
FetchContent_MakeAvailable(glfw imgui md4c)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
add_executable(AdventureBoard
  src/main.cpp
  src/app/Application.cpp
)

target_include_directories(AdventureBoard PRIVATE
  src
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${md4c_SOURCE_DIR}/src
  ${sqlite3_SOURCE_DIR}
  ${stb_SOURCE_DIR}
)

# ImGui sources
target_sources(AdventureBoard PRIVATE
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# MD4C sources
target_sources(AdventureBoard PRIVATE
  ${md4c_SOURCE_DIR}/src/md4c.c
  ${md4c_SOURCE_DIR}/src/entity.c
)

# SQLite amalgamation source
target_sources(AdventureBoard PRIVATE
  ${sqlite3_SOURCE_DIR}/sqlite3.c
)

target_link_libraries(AdventureBoard PRIVATE
  glfw
  glad
  Threads::Threads
  ${CMAKE_DL_LIBS}
)

target_compile_definitions(AdventureBoard PRIVATE
  GLFW_INCLUDE_NONE
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(AdventureBoard PRIVATE -Wall -Wextra)
  # stb/sqlite are C; keep warnings for our own C++ only
endif()

find_package(Threads REQUIRED)