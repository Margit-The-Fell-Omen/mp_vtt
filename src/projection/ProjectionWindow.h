#pragma once

#include "projection/ImageStore.h"

#include <GLFW/glfw3.h>

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

    // Presentation transforms, applied only to the currently projected image.
    // Rotation is in quarter turns; positive = clockwise.
    void rotateImage(int quarterTurns);
    void toggleMirrorH() { m_flipH = !m_flipH; }
    void toggleMirrorV() { m_flipV = !m_flipV; }
    void resetImageTransform();
    int imageRotation() const { return m_rotation; }  // 0..3 (x90 degrees)
    bool mirroredH() const { return m_flipH; }
    bool mirroredV() const { return m_flipV; }

    // Toggles a black overlay (still runs, just hides the image).
    void setBlackout(bool blackout) { m_blackout = blackout; }
    bool blackout() const { return m_blackout; }

    // True when the user asked to close the projection (Esc / window close).
    bool closeRequested() const;

    // Advances/decrements fullscreen presentation.
    // Renders the current image and swaps buffers. No-op when closed.
    void render();

private:
    static void keyCallback(GLFWwindow* window, int key, int scancode,
                            int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action,
                                    int mods);

    void* m_window = nullptr;             // GLFWwindow*
    unsigned int m_texture = 0;           // GL texture id
    std::shared_ptr<Image> m_image;       // current image (CPU pixels)
    int m_currentTextureW = 0;
    int m_currentTextureH = 0;
    bool m_blackout = false;
    bool m_dirty = true;                  // texture needs (re)upload
    bool m_shaderReady = false;
    bool m_closeRequested = false;
    void* m_monitor = nullptr;   // GLFWmonitor* used for the target hint
    int m_fsHintFrames = 0;      // re-send fullscreen-monitor hint after mapping

    // Presentation transforms for the current image.
    int m_rotation = 0;          // quarter turns clockwise
    bool m_flipH = false;
    bool m_flipV = false;

    // GL objects
    unsigned int m_program = 0;
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;

    // "Press Esc / click to close" hint (no-image case).
    unsigned int m_textProgram = 0;
    unsigned int m_textVao = 0;
    unsigned int m_textVbo = 0;
    unsigned int m_textIbo = 0;
};

}  // namespace ab