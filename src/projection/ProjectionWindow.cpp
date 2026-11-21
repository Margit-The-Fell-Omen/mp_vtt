#include "projection/ProjectionWindow.h"

#include <GLFW/glfw3.h>

#if defined(AB_HAVE_X11) && defined(AB_HAVE_XINERAMA)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#endif

#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace ab {

namespace {

#if defined(AB_HAVE_X11) && defined(AB_HAVE_XINERAMA)
// GLFW derives the _NET_WM_FULLSCREEN_MONITORS index from XRandR geometry,
// which is unreliable under XWayland when outputs are scaled (monitors can all
// report position 0,0). Resolve the target monitor to its Xinerama screen by
// logical size and re-send the hint so fullscreen lands on the chosen output.
static void forceFullscreenMonitor(GLFWwindow* window, GLFWmonitor* monitor) {
    if (glfwGetPlatform() != GLFW_PLATFORM_X11) {
        return;
    }
    Display* display = glfwGetX11Display();
    if (!display) {
        return;
    }
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) {
        return;
    }
    float sx = 1.0f;
    float sy = 1.0f;
    glfwGetMonitorContentScale(monitor, &sx, &sy);
    if (sx <= 0.0f) {
        sx = 1.0f;
    }
    if (sy <= 0.0f) {
        sy = 1.0f;
    }
    const int logicalW = static_cast<int>(mode->width / sx + 0.5f);
    const int logicalH = static_cast<int>(mode->height / sy + 0.5f);

    int screenCount = 0;
    XineramaScreenInfo* screens = XineramaQueryScreens(display, &screenCount);
    if (!screens) {
        return;
    }
    int index = -1;
    for (int i = 0; i < screenCount; ++i) {
        if (screens[i].width == logicalW && screens[i].height == logicalH) {
            index = screens[i].screen_number;
            break;
        }
    }
    XFree(screens);
    if (index < 0) {
        return;
    }

    const Window win = glfwGetX11Window(window);
    const Atom atom =
        XInternAtom(display, "_NET_WM_FULLSCREEN_MONITORS", False);
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = win;
    event.xclient.message_type = atom;
    event.xclient.format = 32;
    event.xclient.data.l[0] = index;
    event.xclient.data.l[1] = index;
    event.xclient.data.l[2] = index;
    event.xclient.data.l[3] = index;
    event.xclient.data.l[4] = 0;
    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);
}
#else
static void forceFullscreenMonitor(GLFWwindow*, GLFWmonitor*) {}
#endif

// Hyprland ignores _NET_WM_FULLSCREEN_MONITORS for XWayland windows. Its own
// dispatcher can move the projection (which we focus) to the requested output.
static void moveProjectionToMonitorHyprland(GLFWmonitor* monitor) {
    if (!std::getenv("HYPRLAND_INSTANCE_SIGNATURE")) {
        return;
    }
    const char* name = glfwGetMonitorName(monitor);
    if (!name || !*name) {
        return;
    }
    for (const char* p = name; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (!(std::isalnum(c) || c == '-' || c == '_' || c == '.')) {
            return;
        }
    }
    char cmd[512];
    std::snprintf(cmd, sizeof(cmd),
                  "hyprctl dispatch focuswindow title:Projection "
                  ">/dev/null 2>&1; "
                  "hyprctl dispatch movewindow mon:%s >/dev/null 2>&1",
                  name);
    const int rc = std::system(cmd);
    (void)rc;
}

}  // namespace

ProjectionWindow::~ProjectionWindow() {
    close();
}

void ProjectionWindow::keyCallback(GLFWwindow* window, int key, int scancode,
                                   int action, int mods) {
    (void)scancode;
    (void)mods;
    if (key != GLFW_KEY_ESCAPE || action != GLFW_PRESS) {
        return;
    }
    auto* self =
        static_cast<ProjectionWindow*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_closeRequested = true;
    }
}

void ProjectionWindow::mouseButtonCallback(GLFWwindow* window, int button,
                                           int action, int mods) {
    (void)button;
    (void)mods;
    if (action != GLFW_PRESS) {
        return;
    }
    auto* self =
        static_cast<ProjectionWindow*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_closeRequested = true;
    }
}

bool ProjectionWindow::closeRequested() const {
    if (m_closeRequested) {
        return true;
    }
    if (!m_window) {
        return false;
    }
    return glfwWindowShouldClose(static_cast<GLFWwindow*>(m_window)) ==
           GLFW_TRUE;
}

bool ProjectionWindow::open(int monitorIndex) {
    if (m_window) {
        return true;
    }

    // Creating/setting up the projection context leaves it current; callers
    // (the main UI frame) expect their own context to stay current.
    GLFWwindow* prevContext = glfwGetCurrentContext();

    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    if (monitorIndex < 0 || monitorIndex >= count) {
        std::fprintf(stderr, "Monitor index %d out of range (%d available).\n",
                     monitorIndex, count);
        return false;
    }
    GLFWmonitor* monitor = monitors[monitorIndex];
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) {
        std::fprintf(stderr, "Failed to query the video mode.\n");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    m_window = glfwCreateWindow(mode->width, mode->height, "Projection",
                                monitor, nullptr);
    if (!m_window) {
        std::fprintf(stderr, "Failed to create projection window.\n");
        return false;
    }

    glfwMakeContextCurrent(static_cast<GLFWwindow*>(m_window));
    glfwSwapInterval(1);

    // Esc closes the fullscreen projection so it can never trap the user.
    m_closeRequested = false;
    glfwSetWindowUserPointer(static_cast<GLFWwindow*>(m_window), this);
    glfwSetKeyCallback(static_cast<GLFWwindow*>(m_window),
                       &ProjectionWindow::keyCallback);
    glfwSetMouseButtonCallback(static_cast<GLFWwindow*>(m_window),
                               &ProjectionWindow::mouseButtonCallback);
    glfwFocusWindow(static_cast<GLFWwindow*>(m_window));
    forceFullscreenMonitor(static_cast<GLFWwindow*>(m_window), monitor);
    m_monitor = monitor;
    m_fsHintFrames = 10;

    if (!m_renderer.initialize()) {
        std::fprintf(stderr, "Failed to initialize projection renderer.\n");
        GLFWwindow* win = static_cast<GLFWwindow*>(m_window);
        m_window = nullptr;
        glfwDestroyWindow(win);
        glfwMakeContextCurrent(prevContext);
        return false;
    }
    m_dirty = true;

    glfwMakeContextCurrent(prevContext);
    return true;
}

void ProjectionWindow::close() {
    if (!m_window) {
        return;
    }

    GLFWwindow* win = static_cast<GLFWwindow*>(m_window);
    GLFWwindow* prevContext = glfwGetCurrentContext();
    glfwMakeContextCurrent(win);

    m_renderer.shutdown();
    m_image = nullptr;
    m_dirty = true;
    resetImageTransform();

    m_window = nullptr;
    glfwDestroyWindow(win);

    // Don't restore a destroyed context.
    glfwMakeContextCurrent(prevContext == win ? nullptr : prevContext);
}

void ProjectionWindow::showImage(const std::shared_ptr<Image>& image) {
    if (m_image.get() == image.get() && !m_dirty) {
        return;
    }
    if (m_image.get() != image.get()) {
        // Transforms belong to the image currently being projected.
        resetImageTransform();
    }
    m_image = image;
    m_dirty = true;
}

void ProjectionWindow::clearImage() {
    m_image = nullptr;
    m_dirty = true;
    resetImageTransform();
}

void ProjectionWindow::rotateImage(int quarterTurns) {
    m_rotation = ((m_rotation + quarterTurns) % 4 + 4) % 4;
}

void ProjectionWindow::resetImageTransform() {
    m_rotation = 0;
    m_flipH = false;
    m_flipV = false;
}

void ProjectionWindow::render() {
    if (!m_window || !m_renderer.isInitialized()) {
        return;
    }

    GLFWwindow* window = static_cast<GLFWwindow*>(m_window);
    GLFWwindow* prevContext = glfwGetCurrentContext();
    glfwMakeContextCurrent(window);

    // The WM may apply the initial fullscreen hint before the window is mapped;
    // repeat it for a few frames so it wins.
    if (m_fsHintFrames > 0 && m_monitor) {
        auto* monitor = static_cast<GLFWmonitor*>(m_monitor);
        forceFullscreenMonitor(window, monitor);
        // On Hyprland the EWMH hint is ignored; move the focused window with
        // the compositor's own dispatcher once it is mapped.
        if (m_fsHintFrames == 8 || m_fsHintFrames == 4) {
            moveProjectionToMonitorHyprland(monitor);
        }
        --m_fsHintFrames;
    }

    int W = 0;
    int H = 0;
    glfwGetFramebufferSize(window, &W, &H);
    m_renderer.render(W, H, m_image.get(), m_blackout, m_rotation, m_flipH,
                      m_flipV, m_dirty);
    m_dirty = false;

    glfwSwapBuffers(window);
    glfwMakeContextCurrent(prevContext);
}
}  // namespace ab