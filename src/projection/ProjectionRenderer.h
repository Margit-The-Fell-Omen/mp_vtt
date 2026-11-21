#pragma once

#include "projection/ImageStore.h"

namespace ab {

// Shared OpenGL/OpenGL ES projection pass: image quad, transforms, blackout,
// and the no-image hint. The caller owns the context/window/surface.
class ProjectionRenderer {
public:
    // Compiles shaders and creates GL objects. Requires a current context.
    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // Renders one frame. `upload` forces the image texture to be refreshed.
    void render(int width, int height, const Image* image, bool blackout,
                int rotation, bool flipH, bool flipV, bool upload);

private:
    unsigned int m_program = 0;
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;

    unsigned int m_textProgram = 0;
    unsigned int m_textVao = 0;
    unsigned int m_textVbo = 0;
    unsigned int m_textIbo = 0;

    unsigned int m_texture = 0;
    int m_textureW = 0;
    int m_textureH = 0;
    bool m_initialized = false;
};

}  // namespace ab
