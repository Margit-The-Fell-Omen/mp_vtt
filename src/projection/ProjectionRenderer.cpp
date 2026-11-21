#include "projection/ProjectionRenderer.h"

#include "platform/GL.h"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STB_EASY_FONT_IMPLEMENTATION
#include <stb_easy_font.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <cstdio>

namespace ab {

namespace {

#if defined(__ANDROID__)
const char* kVertShader = R"GLSL(
#version 300 es
precision highp float;
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vUV = aUV;
}
)GLSL";

const char* kFragShader = R"GLSL(
#version 300 es
precision highp float;
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
void main() {
    FragColor = texture(uTex, vUV);
}
)GLSL";

const char* kTextVertShader = R"GLSL(
#version 300 es
precision highp float;
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
#version 300 es
precision highp float;
out vec4 FragColor;
uniform vec4 uColor;
void main() {
    FragColor = uColor;
}
)GLSL";
#else
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
#endif

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

}  // namespace

bool ProjectionRenderer::initialize() {
    if (m_initialized) {
        return true;
    }

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
    m_initialized = true;
    return true;
}

void ProjectionRenderer::shutdown() {
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
    m_textureW = 0;
    m_textureH = 0;
    m_initialized = false;
}

void ProjectionRenderer::render(int width, int height, const Image* image,
                                bool blackout, int rotation, bool flipH,
                                bool flipV, bool upload) {
    if (!m_initialized || width <= 0 || height <= 0) {
        return;
    }

    // Upload the (new) image into the texture.
    if (upload && image && !image->pixels.empty()) {
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image->width, image->height,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels.data());
        m_textureW = image->width;
        m_textureH = image->height;
    }

    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (image && !blackout) {
        float quad[6 * 4];
        buildQuad(static_cast<float>(width), static_cast<float>(height),
                  static_cast<float>(m_textureW),
                  static_cast<float>(m_textureH), rotation, flipH, flipV,
                  quad);

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

    // With no image the surface is just black; show how to get out of it.
    if (!image && !blackout) {
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
                        static_cast<float>(width), static_cast<float>(height));
            const float textScale =
                (static_cast<float>(height) / 540.0f < 1.0f)
                    ? 1.0f
                    : static_cast<float>(height) / 540.0f;
            glUniform1f(glGetUniformLocation(m_textProgram, "uScale"),
                        textScale);
            glUniform4f(glGetUniformLocation(m_textProgram, "uColor"), 1.0f,
                        1.0f, 1.0f, 1.0f);
            glBindVertexArray(m_textVao);
            glDrawElements(GL_TRIANGLES, quads * 6, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        }
    }
}

}  // namespace ab
