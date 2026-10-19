#include "projection/ProjectionWindow.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdio>

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

// Fills `out` with 6 vertices (pos.xy, uv.xy) forming an aspect-fit quad.
void buildQuad(float screenW, float screenH, float imgW, float imgH,
               float* out) {
    float scale = (imgW > 0 && imgH > 0)
                      ? (screenW / imgW < screenH / imgH ? screenW / imgW
                                                          : screenH / imgH)
                      : 1.0f;
    float hw = (imgW * scale) / screenW;  // half extent in NDC x
    float hh = (imgH * scale) / screenH;  // half extent in NDC y

    // pos.xy, uv.xy per vertex (triangle strip as 2 triangles)
    const float verts[6][4] = {
        {-hw, -hh, 0.0f, 0.0f},
        { hw, -hh, 1.0f, 0.0f},
        { hw,  hh, 1.0f, 1.0f},
        {-hw, -hh, 0.0f, 0.0f},
        { hw,  hh, 1.0f, 1.0f},
        {-hw,  hh, 0.0f, 1.0f},
    };
    for (int i = 0; i < 6; ++i) {
        out[i * 4 + 0] = verts[i][0];
        out[i * 4 + 1] = verts[i][1];
        out[i * 4 + 2] = verts[i][2];
        out[i * 4 + 3] = verts[i][3];
    }
}

}  // namespace

ProjectionWindow::~ProjectionWindow() {
    close();
}

bool ProjectionWindow::open(int monitorIndex) {
    if (m_window) {
        return true;
    }

    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    if (monitorIndex < 0 || monitorIndex >= count) {
        std::fprintf(stderr, "Monitor index %d out of range (%d available).\n",
                     monitorIndex, count);
        return false;
    }
    GLFWmonitor* monitor = monitors[monitorIndex];
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
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

    glGenTextures(1, &m_texture);
    m_shaderReady = true;
    m_dirty = true;
    return true;
}

void ProjectionWindow::close() {
    if (!m_window) {
        return;
    }
    glfwMakeContextCurrent(static_cast<GLFWwindow*>(m_window));

    if (m_texture) glDeleteTextures(1, &m_texture);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_program) glDeleteProgram(m_program);
    m_texture = m_vbo = m_vao = m_program = 0;
    m_shaderReady = false;
    m_image = nullptr;
    m_dirty = true;

    glfwDestroyWindow(static_cast<GLFWwindow*>(m_window));
    m_window = nullptr;
}

void ProjectionWindow::showImage(const std::shared_ptr<Image>& image) {
    if (m_image.get() == image.get() && !m_dirty) {
        return;
    }
    m_image = image;
    m_dirty = true;
}

void ProjectionWindow::clearImage() {
    m_image = nullptr;
    m_dirty = true;
}

void ProjectionWindow::render() {
    if (!m_window || !m_shaderReady) {
        return;
    }

    GLFWwindow* window = static_cast<GLFWwindow*>(m_window);
    glfwMakeContextCurrent(window);

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
                  static_cast<float>(m_currentTextureH), quad);

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

    glfwSwapBuffers(window);
}

}  // namespace ab