#pragma once

// Renderer-facing GL entry point. Desktop loads the desktop OpenGL 3.3 API
// through glad; Android uses the platform OpenGL ES 3 headers directly.
#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
