#include "projection/ProjectionWindow.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STB_EASY_FONT_IMPLEMENTATION
#include <stb_easy_font.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

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

const char* kVertShader = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vUV = aUV;
}
)GLSL";

const char* kFragShader = R"GLSL(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
void main() {
    FragColor = texture(uTex, vUV);
}
)GLSL";

const char* kTextVertShader = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec2 uScreen;
uniform float uScale;
void main() {
    vec2 p = aPos * uScale;
    vec2 ndc = vec2(p.x / uScreen.x * 2.0 - 1.0,
                    1.0 - p.y / uScreen.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)GLSL";

const char* kTextFragShader = R"GLSL(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() {
    FragColor = uColor;
}
)GLSL";

constexpr int kMaxHintQuads = 512;

unsigned int compileShader(unsigned int type, const char* src) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::fprintf(stderr, "Shader compile error: %s\n", log);
    }
    return shader;
}

// Applies mirroring and 90-degree rotation to a texture coordinate.
void transformUV(float& u, float& v, int rotation, bool flipH, bool flipV) {
    if (flipH) {
        u = 1.0f - u;
    }
    if (flipV) {
        v = 1.0f - v;
    }
    switch (rotation & 3) {
        case 1: {  // 90 degrees clockwise (inverse map: display -> source)
            const float nu = 1.0f - v;
            v = u;
            u = nu;
            break;
        }
        case 2:  // 180 degrees
            u = 1.0f - u;
            v = 1.0f - v;
            break;
        case 3: {  // 270 degrees clockwise (90 counter-clockwise)
            const float nu = v;
            v = 1.0f - u;
            u = nu;
            break;
        }
        default:
            break;
    }
}

// Fills `out` with 6 vertices (pos.xy, uv.xy) forming an aspect-fit quad.
void buildQuad(float screenW, float screenH, float imgW, float imgH,
               int rotation, bool flipH, bool flipV, float* out) {
    // Odd quarter turns swap the displayed width/height.
    const bool swap = (rotation & 1) != 0;
    const float dispW = swap ? imgH : imgW;
    const float dispH = swap ? imgW : imgH;
    float scale = (dispW > 0 && dispH > 0)
                      ? (screenW / dispW < screenH / dispH ? screenW / dispW
                                                          : screenH / dispH)
                      : 1.0f;
    float hw = (dispW * scale) / screenW;  // half extent in NDC x
    float hh = (dispH * scale) / screenH;  // half extent in NDC y

    const float pos[4][2] = {
        {-hw, -hh},
        { hw, -hh},
        { hw,  hh},
        {-hw,  hh},
    };
    const float uv[4][2] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
    };
    static const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; ++i) {
        const int c = order[i];
        float u = uv[c][0];
        float v = uv[c][1];
        transformUV(u, v, rotation, flipH, flipV);
        out[i * 4 + 0] = pos[c][0];
        out[i * 4 + 1] = pos[c][1];
        out[i * 4 + 2] = u;
        out[i * 4 + 3] = v;
    }
}

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

    // One-time GL setup (in this context).
    unsigned int vs = compileShader(GL_VERTEX_SHADER, kVertShader);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, kFragShader);
    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr,
                 GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);

    // Hint text (shown when no image is projected).
    unsigned int tvs = compileShader(GL_VERTEX_SHADER, kTextVertShader);
    unsigned int tfs = compileShader(GL_FRAGMENT_SHADER, kTextFragShader);
    m_textProgram = glCreateProgram();
    glAttachShader(m_textProgram, tvs);
    glAttachShader(m_textProgram, tfs);
    glLinkProgram(m_textProgram);
    glDeleteShader(tvs);
    glDeleteShader(tfs);

    glGenVertexArrays(1, &m_textVao);
    glGenBuffers(1, &m_textVbo);
    glGenBuffers(1, &m_textIbo);
    glBindVertexArray(m_textVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_textVbo);
    glBufferData(GL_ARRAY_BUFFER, kMaxHintQuads * 4 * 16, nullptr,
                 GL_DYNAMIC_DRAW);
    unsigned int indices[kMaxHintQuads * 6];
    for (int q = 0; q < kMaxHintQuads; ++q) {
        const unsigned int b = static_cast<unsigned int>(q * 4);
        indices[q * 6 + 0] = b + 0;
        indices[q * 6 + 1] = b + 1;
        indices[q * 6 + 2] = b + 2;
        indices[q * 6 + 3] = b + 0;
        indices[q * 6 + 4] = b + 2;
        indices[q * 6 + 5] = b + 3;
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_textIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16,
                          reinterpret_cast<void*>(0));
    glBindVertexArray(0);

    glGenTextures(1, &m_texture);
    m_shaderReady = true;
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

    if (m_texture) glDeleteTextures(1, &m_texture);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_program) glDeleteProgram(m_program);
    if (m_textIbo) glDeleteBuffers(1, &m_textIbo);
    if (m_textVbo) glDeleteBuffers(1, &m_textVbo);
    if (m_textVao) glDeleteVertexArrays(1, &m_textVao);
    if (m_textProgram) glDeleteProgram(m_textProgram);
    m_texture = m_vbo = m_vao = m_program = 0;
    m_textIbo = m_textVbo = m_textVao = m_textProgram = 0;
    m_shaderReady = false;
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
    if (!m_window || !m_shaderReady) {
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

    // Upload the (new) image into the texture.
    if (m_dirty) {
        if (m_image && !m_image->pixels.empty()) {
            glBindTexture(GL_TEXTURE_2D, m_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_image->width,
                         m_image->height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         m_image->pixels.data());
            m_currentTextureW = m_image->width;
            m_currentTextureH = m_image->height;
        }
        m_dirty = false;
    }

    glViewport(0, 0, W, H);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_image && !m_blackout) {
        float quad[6 * 4];
        buildQuad(static_cast<float>(W), static_cast<float>(H),
                  static_cast<float>(m_currentTextureW),
                  static_cast<float>(m_currentTextureH), m_rotation, m_flipH,
                  m_flipV, quad);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);

        glUseProgram(m_program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glUniform1i(glGetUniformLocation(m_program, "uTex"), 0);

        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    // With no image the window is just black; show how to get out of it.
    if (!m_image && !m_blackout) {
        static char hint[] = "Press Esc or click to close projection";
        static char textBuf[kMaxHintQuads * 4 * 16];
        int quads = stb_easy_font_print(12.0f, 28.0f, hint, nullptr, textBuf,
                                        sizeof(textBuf));
        if (quads > kMaxHintQuads) {
            quads = kMaxHintQuads;
        }
        if (quads > 0) {
            glBindBuffer(GL_ARRAY_BUFFER, m_textVbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0,
                            static_cast<GLsizeiptr>(quads) * 4 * 16, textBuf);
            glUseProgram(m_textProgram);
            glUniform2f(glGetUniformLocation(m_textProgram, "uScreen"),
                        static_cast<float>(W), static_cast<float>(H));
            const float textScale =
                (static_cast<float>(H) / 540.0f < 1.0f)
                    ? 1.0f
                    : static_cast<float>(H) / 540.0f;
            glUniform1f(glGetUniformLocation(m_textProgram, "uScale"),
                        textScale);
            glUniform4f(glGetUniformLocation(m_textProgram, "uColor"), 1.0f,
                        1.0f, 1.0f, 1.0f);
            glBindVertexArray(m_textVao);
            glDrawElements(GL_TRIANGLES, quads * 6, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        }
    }

    glfwSwapBuffers(window);
    glfwMakeContextCurrent(prevContext);
}

}  // namespace ab