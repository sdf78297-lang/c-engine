#include <ExoEngine/Renderer/Renderer.h>

#include <ExoEngine/Core/Logger.h>

#include <GL/glew.h>

#include <array>
#include <string>
#include <vector>

namespace Exo {

namespace {

constexpr const char* kVertexShader = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 uViewProjection;
uniform mat4 uModel;

out vec3 vColor;

void main() {
    vColor = aColor;
    gl_Position = uViewProjection * uModel * vec4(aPos, 1.0);
}
)glsl";

constexpr const char* kFragmentShader = R"glsl(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 1.0);
}
)glsl";

std::uint32_t compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string info(static_cast<std::size_t>(length), '\0');
        glGetShaderInfoLog(shader, length, nullptr, info.data());
        Logger::error(info);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

std::uint32_t linkProgram(std::uint32_t vertexShader, std::uint32_t fragmentShader) {
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string info(static_cast<std::size_t>(length), '\0');
        glGetProgramInfoLog(program, length, nullptr, info.data());
        Logger::error(info);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

} // namespace

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize(std::uint32_t width, std::uint32_t height) {
    width_ = width;
    height_ = height;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    resize(width, height);

    if (!createReferenceShader() || !createReferenceGeometry()) {
        shutdown();
        return false;
    }

    Logger::info("OpenGL renderer initialized");
    return true;
}

void Renderer::shutdown() {
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }

    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }

    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }

    if (shader_ != 0) {
        glDeleteProgram(shader_);
        shader_ = 0;
    }
}

void Renderer::resize(std::uint32_t width, std::uint32_t height) {
    width_ = width;
    height_ = height == 0 ? 1 : height;
    glViewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
}

void Renderer::beginFrame(const RenderView& view) {
    stats_.drawCalls = 0;
    ++stats_.frameIndex;
    currentViewProjection_ = view.projection * view.view;

    glClearColor(0.018f, 0.021f, 0.023f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::drawReferenceRoom() {
    if (shader_ == 0 || vao_ == 0) {
        return;
    }

    glUseProgram(shader_);
    glBindVertexArray(vao_);

    const Mat4 model = Mat4::identity();
    const GLint vpLocation = glGetUniformLocation(shader_, "uViewProjection");
    const GLint modelLocation = glGetUniformLocation(shader_, "uModel");
    glUniformMatrix4fv(vpLocation, 1, GL_FALSE, currentViewProjection_.data());
    glUniformMatrix4fv(modelLocation, 1, GL_FALSE, model.data());

    glDrawElements(GL_TRIANGLES, 30, GL_UNSIGNED_INT, nullptr);
    ++stats_.drawCalls;

    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::endFrame() {}

const RenderStats& Renderer::stats() const {
    return stats_;
}

bool Renderer::createReferenceGeometry() {
    struct Vertex {
        Vec3 position;
        Vec3 color;
    };

    const std::array<Vertex, 8> vertices {{
        {{-2.0f, 0.0f, -2.0f}, {0.17f, 0.20f, 0.20f}},
        {{2.0f, 0.0f, -2.0f}, {0.17f, 0.20f, 0.20f}},
        {{2.0f, 0.0f, 2.0f}, {0.15f, 0.17f, 0.17f}},
        {{-2.0f, 0.0f, 2.0f}, {0.15f, 0.17f, 0.17f}},
        {{-2.0f, 2.4f, -2.0f}, {0.35f, 0.36f, 0.34f}},
        {{2.0f, 2.4f, -2.0f}, {0.34f, 0.35f, 0.33f}},
        {{2.0f, 2.4f, 2.0f}, {0.30f, 0.31f, 0.30f}},
        {{-2.0f, 2.4f, 2.0f}, {0.31f, 0.32f, 0.31f}},
    }};

    const std::array<std::uint32_t, 30> indices {{
        0, 1, 2, 0, 2, 3,
        0, 4, 5, 0, 5, 1,
        1, 5, 6, 1, 6, 2,
        2, 6, 7, 2, 7, 3,
        3, 7, 4, 3, 4, 0,
    }};

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(vertices)), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(indices)), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color)));

    glBindVertexArray(0);
    return true;
}

bool Renderer::createReferenceShader() {
    const std::uint32_t vertexShader = compileShader(GL_VERTEX_SHADER, kVertexShader);
    if (vertexShader == 0) {
        return false;
    }

    const std::uint32_t fragmentShader = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    shader_ = linkProgram(vertexShader, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shader_ != 0;
}

} // namespace Exo
