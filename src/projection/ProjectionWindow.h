#pragma once

#include "projection/ImageStore.h"

#include <memory>

namespace ab {

// A separate fullscreen window (second monitor/TV) that projects an image.
// Owns its own GLFW window + OpenGL context, independent of the main UI.
class ProjectionWindow {
public:
    ~ProjectionWindow();

    // Opens a fullscreen window on the given monitor index.
    bool open(int monitorIndex);
    void close();
    bool isOpen() const { return m_window != nullptr; }

    // Sets the image to display (uploads the texture when content changes).
    void showImage(const std::shared_ptr<Image>& image);
    void clearImage();
    bool hasImage() const { return m_image != nullptr; }

    // Toggles a black overlay (still runs, just hides the image).
    void setBlackout(bool blackout) { m_blackout = blackout; }
    bool blackout() const { return m_blackout; }

    // Advances/decrements fullscreen presentation.
    // Renders the current image and swaps buffers. No-op when closed.
    void render();

private:
    void* m_window = nullptr;             // GLFWwindow*
    unsigned int m_texture = 0;           // GL texture id
    std::shared_ptr<Image> m_image;       // current image (CPU pixels)
    int m_currentTextureW = 0;
    int m_currentTextureH = 0;
    bool m_blackout = false;
    bool m_dirty = true;                  // texture needs (re)upload
    bool m_shaderReady = false;

    // GL objects
    unsigned int m_program = 0;
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
};

}  // namespace ab