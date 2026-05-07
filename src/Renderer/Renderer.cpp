#include <ExoEngine/Renderer/Renderer.h>

#include <ExoEngine/Assets/GltfLoader.h>
#include <ExoEngine/Core/Logger.h>

#include <GL/glew.h>

#include <algorithm>
#include <array>
#include <stdexcept>
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

constexpr const char* kTexturedVertexShader = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uViewProjection;
uniform mat4 uModel;

out vec3 vNormalWS;
out vec2 vUV;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vNormalWS = normalize(mat3(uModel) * aNormal);
    vUV = aUV;
    gl_Position = uViewProjection * worldPos;
}
)glsl";

constexpr const char* kTexturedFragmentShader = R"glsl(
#version 330 core
in vec3 vNormalWS;
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uAlbedo;
uniform vec3 uBaseColor;

void main() {
    vec4 sampled = texture(uAlbedo, vUV);
    vec3 albedo = sampled.rgb * uBaseColor;

    vec3 lightDir = normalize(vec3(0.40, 0.85, 0.32));
    float ndotl = max(dot(normalize(vNormalWS), lightDir), 0.0);
    float ambient = 0.30;
    vec3 lit = albedo * (ambient + ndotl * 0.85);

    FragColor = vec4(lit, sampled.a);
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

    if (!createReferenceShader() || !createReferenceGeometry() || !createCharacterGeometry() || !createTVGeometry() || !createTexturedShader()) {
        shutdown();
        return false;
    }

    Logger::info("OpenGL renderer initialized");
    return true;
}

void Renderer::shutdown() {
    for (GltfModel& model : gltfModels_) {
        for (GltfSubMesh& sub : model.subMeshes) {
            if (sub.ebo != 0) {
                glDeleteBuffers(1, &sub.ebo);
            }
            if (sub.vbo != 0) {
                glDeleteBuffers(1, &sub.vbo);
            }
            if (sub.vao != 0) {
                glDeleteVertexArrays(1, &sub.vao);
            }
            if (sub.texture != 0 && sub.ownsTexture) {
                glDeleteTextures(1, &sub.texture);
            }
        }
    }
    gltfModels_.clear();

    if (whiteTexture_ != 0) {
        glDeleteTextures(1, &whiteTexture_);
        whiteTexture_ = 0;
    }

    if (texturedShader_ != 0) {
        glDeleteProgram(texturedShader_);
        texturedShader_ = 0;
    }

    if (tvEbo_ != 0) {
        glDeleteBuffers(1, &tvEbo_);
        tvEbo_ = 0;
    }

    if (tvVbo_ != 0) {
        glDeleteBuffers(1, &tvVbo_);
        tvVbo_ = 0;
    }

    if (tvVao_ != 0) {
        glDeleteVertexArrays(1, &tvVao_);
        tvVao_ = 0;
    }

    tvIndexCount_ = 0;

    if (characterEbo_ != 0) {
        glDeleteBuffers(1, &characterEbo_);
        characterEbo_ = 0;
    }

    if (characterVbo_ != 0) {
        glDeleteBuffers(1, &characterVbo_);
        characterVbo_ = 0;
    }

    if (characterVao_ != 0) {
        glDeleteVertexArrays(1, &characterVao_);
        characterVao_ = 0;
    }

    characterIndexCount_ = 0;

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

    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
    ++stats_.drawCalls;

    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::drawCharacter(const Mat4& modelTransform) {
    if (shader_ == 0 || characterVao_ == 0 || characterIndexCount_ == 0) {
        return;
    }

    glUseProgram(shader_);
    glBindVertexArray(characterVao_);

    const GLint vpLocation = glGetUniformLocation(shader_, "uViewProjection");
    const GLint modelLocation = glGetUniformLocation(shader_, "uModel");
    glUniformMatrix4fv(vpLocation, 1, GL_FALSE, currentViewProjection_.data());
    glUniformMatrix4fv(modelLocation, 1, GL_FALSE, modelTransform.data());

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(characterIndexCount_), GL_UNSIGNED_INT, nullptr);
    ++stats_.drawCalls;

    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::drawTV(const Mat4& modelTransform) {
    if (shader_ == 0 || tvVao_ == 0 || tvIndexCount_ == 0) {
        return;
    }

    glUseProgram(shader_);
    glBindVertexArray(tvVao_);

    const GLint vpLocation = glGetUniformLocation(shader_, "uViewProjection");
    const GLint modelLocation = glGetUniformLocation(shader_, "uModel");
    glUniformMatrix4fv(vpLocation, 1, GL_FALSE, currentViewProjection_.data());
    glUniformMatrix4fv(modelLocation, 1, GL_FALSE, modelTransform.data());

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(tvIndexCount_), GL_UNSIGNED_INT, nullptr);
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

    const std::array<Vertex, 24> vertices {{
        {{-2.0f, 0.0f, -2.0f}, {0.13f, 0.16f, 0.16f}},
        {{-2.0f, 0.0f, 2.0f}, {0.14f, 0.17f, 0.17f}},
        {{2.0f, 0.0f, 2.0f}, {0.15f, 0.18f, 0.18f}},
        {{2.0f, 0.0f, -2.0f}, {0.13f, 0.16f, 0.16f}},

        {{-2.0f, 2.4f, -2.0f}, {0.27f, 0.28f, 0.26f}},
        {{2.0f, 2.4f, -2.0f}, {0.28f, 0.29f, 0.27f}},
        {{2.0f, 2.4f, 2.0f}, {0.25f, 0.26f, 0.25f}},
        {{-2.0f, 2.4f, 2.0f}, {0.25f, 0.26f, 0.25f}},

        {{-2.0f, 0.0f, -2.0f}, {0.38f, 0.39f, 0.36f}},
        {{2.0f, 0.0f, -2.0f}, {0.39f, 0.40f, 0.37f}},
        {{2.0f, 2.4f, -2.0f}, {0.31f, 0.32f, 0.30f}},
        {{-2.0f, 2.4f, -2.0f}, {0.30f, 0.31f, 0.29f}},

        {{-2.0f, 0.0f, 2.0f}, {0.34f, 0.35f, 0.33f}},
        {{-2.0f, 2.4f, 2.0f}, {0.28f, 0.29f, 0.27f}},
        {{2.0f, 2.4f, 2.0f}, {0.29f, 0.30f, 0.28f}},
        {{2.0f, 0.0f, 2.0f}, {0.35f, 0.36f, 0.34f}},

        {{-2.0f, 0.0f, -2.0f}, {0.33f, 0.35f, 0.34f}},
        {{-2.0f, 2.4f, -2.0f}, {0.27f, 0.29f, 0.28f}},
        {{-2.0f, 2.4f, 2.0f}, {0.28f, 0.30f, 0.29f}},
        {{-2.0f, 0.0f, 2.0f}, {0.34f, 0.36f, 0.35f}},

        {{2.0f, 0.0f, -2.0f}, {0.32f, 0.33f, 0.31f}},
        {{2.0f, 0.0f, 2.0f}, {0.33f, 0.34f, 0.32f}},
        {{2.0f, 2.4f, 2.0f}, {0.27f, 0.28f, 0.26f}},
        {{2.0f, 2.4f, -2.0f}, {0.26f, 0.27f, 0.25f}},
    }};

    const std::array<std::uint32_t, 36> indices {{
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23,
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

bool Renderer::createCharacterGeometry() {
    struct Vertex {
        Vec3 position;
        Vec3 color;
    };

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve(6 * 24);
    indices.reserve(6 * 36);

    auto appendBox = [&](Vec3 minCorner, Vec3 maxCorner, Vec3 color) {
        const std::array<Vec3, 8> corners {{
            {minCorner.x, minCorner.y, minCorner.z},
            {maxCorner.x, minCorner.y, minCorner.z},
            {maxCorner.x, maxCorner.y, minCorner.z},
            {minCorner.x, maxCorner.y, minCorner.z},
            {minCorner.x, minCorner.y, maxCorner.z},
            {maxCorner.x, minCorner.y, maxCorner.z},
            {maxCorner.x, maxCorner.y, maxCorner.z},
            {minCorner.x, maxCorner.y, maxCorner.z},
        }};

        constexpr int faceCorners[6][4] = {
            {3, 7, 6, 2}, // top    +Y
            {0, 1, 5, 4}, // bottom -Y
            {4, 5, 6, 7}, // front  +Z
            {1, 0, 3, 2}, // back   -Z
            {5, 1, 2, 6}, // right  +X
            {0, 4, 7, 3}, // left   -X
        };

        constexpr float faceTints[6] = {1.05f, 0.78f, 1.00f, 0.88f, 0.95f, 0.92f};

        for (int face = 0; face < 6; ++face) {
            const float tint = faceTints[face];
            const Vec3 shaded {color.x * tint, color.y * tint, color.z * tint};
            const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
            for (int corner = 0; corner < 4; ++corner) {
                vertices.push_back({corners[faceCorners[face][corner]], shaded});
            }
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        }
    };

    constexpr Vec3 skinColor {0.79f, 0.65f, 0.55f};
    constexpr Vec3 torsoColor {0.40f, 0.32f, 0.45f};
    constexpr Vec3 legColor {0.22f, 0.26f, 0.30f};

    appendBox({-0.14f, 1.41f, -0.13f}, {0.14f, 1.69f, 0.13f}, skinColor);   // head
    appendBox({-0.25f, 0.79f, -0.14f}, {0.25f, 1.41f, 0.14f}, torsoColor);  // torso
    appendBox({-0.23f, 0.00f, -0.11f}, {-0.03f, 0.80f, 0.11f}, legColor);   // left leg
    appendBox({0.03f, 0.00f, -0.11f}, {0.23f, 0.80f, 0.11f}, legColor);     // right leg
    appendBox({-0.43f, 0.78f, -0.07f}, {-0.29f, 1.32f, 0.07f}, skinColor);  // left arm
    appendBox({0.29f, 0.78f, -0.07f}, {0.43f, 1.32f, 0.07f}, skinColor);    // right arm

    characterIndexCount_ = static_cast<std::uint32_t>(indices.size());

    glGenVertexArrays(1, &characterVao_);
    glGenBuffers(1, &characterVbo_);
    glGenBuffers(1, &characterEbo_);

    glBindVertexArray(characterVao_);
    glBindBuffer(GL_ARRAY_BUFFER, characterVbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
        vertices.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, characterEbo_);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
        indices.data(),
        GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color)));

    glBindVertexArray(0);
    return true;
}

bool Renderer::createTVGeometry() {
    struct Vertex {
        Vec3 position;
        Vec3 color;
    };

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve(2 * 24);
    indices.reserve(2 * 36);

    auto appendBox = [&](Vec3 minCorner, Vec3 maxCorner, Vec3 color) {
        const std::array<Vec3, 8> corners {{
            {minCorner.x, minCorner.y, minCorner.z},
            {maxCorner.x, minCorner.y, minCorner.z},
            {maxCorner.x, maxCorner.y, minCorner.z},
            {minCorner.x, maxCorner.y, minCorner.z},
            {minCorner.x, minCorner.y, maxCorner.z},
            {maxCorner.x, minCorner.y, maxCorner.z},
            {maxCorner.x, maxCorner.y, maxCorner.z},
            {minCorner.x, maxCorner.y, maxCorner.z},
        }};

        constexpr int faceCorners[6][4] = {
            {3, 7, 6, 2},
            {0, 1, 5, 4},
            {4, 5, 6, 7},
            {1, 0, 3, 2},
            {5, 1, 2, 6},
            {0, 4, 7, 3},
        };

        constexpr float faceTints[6] = {1.05f, 0.78f, 1.00f, 0.88f, 0.95f, 0.92f};

        for (int face = 0; face < 6; ++face) {
            const float tint = faceTints[face];
            const Vec3 shaded {color.x * tint, color.y * tint, color.z * tint};
            const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
            for (int corner = 0; corner < 4; ++corner) {
                vertices.push_back({corners[faceCorners[face][corner]], shaded});
            }
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        }
    };

    constexpr Vec3 caseColor {0.74f, 0.70f, 0.60f};
    constexpr Vec3 screenColor {0.05f, 0.05f, 0.07f};

    appendBox({-0.275f, 0.0f, -0.225f}, {0.275f, 0.42f, 0.225f}, caseColor);   // body
    appendBox({-0.21f, 0.06f, 0.225f}, {0.21f, 0.36f, 0.235f}, screenColor);   // screen plate

    tvIndexCount_ = static_cast<std::uint32_t>(indices.size());

    glGenVertexArrays(1, &tvVao_);
    glGenBuffers(1, &tvVbo_);
    glGenBuffers(1, &tvEbo_);

    glBindVertexArray(tvVao_);
    glBindBuffer(GL_ARRAY_BUFFER, tvVbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
        vertices.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tvEbo_);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
        indices.data(),
        GL_STATIC_DRAW);

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

bool Renderer::createTexturedShader() {
    const std::uint32_t vertexShader = compileShader(GL_VERTEX_SHADER, kTexturedVertexShader);
    if (vertexShader == 0) {
        return false;
    }

    const std::uint32_t fragmentShader = compileShader(GL_FRAGMENT_SHADER, kTexturedFragmentShader);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    texturedShader_ = linkProgram(vertexShader, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (texturedShader_ == 0) {
        return false;
    }

    glGenTextures(1, &whiteTexture_);
    glBindTexture(GL_TEXTURE_2D, whiteTexture_);
    constexpr std::uint8_t white[] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

std::int32_t Renderer::loadGltfMesh(const std::filesystem::path& path) {
    GltfModelData data;
    try {
        data = GltfLoader::loadFromFile(path);
    } catch (const std::exception& error) {
        Logger::error(std::string("GLB load failed: ") + error.what());
        return -1;
    }

    if (data.primitives.empty()) {
        Logger::warn("GLB contains no usable primitives: " + path.string());
        return -1;
    }

    GltfModel model;
    model.subMeshes.reserve(data.primitives.size());
    model.aabbMin = data.aabbMin;
    model.aabbMax = data.aabbMax;

    std::uint64_t totalVerts = 0;
    std::uint64_t totalTris = 0;

    for (const GltfPrimitiveData& prim : data.primitives) {
        GltfSubMesh sub;
        sub.indexCount = static_cast<std::uint32_t>(prim.indices.size());
        sub.baseColor = prim.baseColorFactor;

        glGenVertexArrays(1, &sub.vao);
        glGenBuffers(1, &sub.vbo);
        glGenBuffers(1, &sub.ebo);

        glBindVertexArray(sub.vao);

        glBindBuffer(GL_ARRAY_BUFFER, sub.vbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(prim.vertices.size() * sizeof(GltfVertex)),
            prim.vertices.data(),
            GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sub.ebo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(prim.indices.size() * sizeof(std::uint32_t)),
            prim.indices.data(),
            GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GltfVertex), reinterpret_cast<void*>(offsetof(GltfVertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GltfVertex), reinterpret_cast<void*>(offsetof(GltfVertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GltfVertex), reinterpret_cast<void*>(offsetof(GltfVertex, uv)));

        glBindVertexArray(0);

        if (!prim.textureRgba.empty()) {
            glGenTextures(1, &sub.texture);
            sub.ownsTexture = true;
            glBindTexture(GL_TEXTURE_2D, sub.texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGBA8,
                prim.textureWidth,
                prim.textureHeight,
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                prim.textureRgba.data());
            glGenerateMipmap(GL_TEXTURE_2D);

            if (GLEW_EXT_texture_filter_anisotropic) {
                GLfloat maxAniso = 1.0f;
                glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(16.0f, maxAniso));
            }

            glBindTexture(GL_TEXTURE_2D, 0);
        } else {
            sub.texture = whiteTexture_;
            sub.ownsTexture = false;
        }

        totalVerts += prim.vertices.size();
        totalTris += prim.indices.size() / 3;
        model.subMeshes.push_back(sub);
    }

    gltfModels_.push_back(std::move(model));
    Logger::info("GLB loaded: " + path.filename().string()
        + " (" + std::to_string(data.primitives.size()) + " prims, "
        + std::to_string(totalVerts) + " verts, "
        + std::to_string(totalTris) + " tris)");
    return static_cast<std::int32_t>(gltfModels_.size() - 1);
}

void Renderer::drawGltfMesh(std::int32_t handle, const Mat4& modelTransform) {
    if (handle < 0 || static_cast<std::size_t>(handle) >= gltfModels_.size()) {
        return;
    }
    if (texturedShader_ == 0) {
        return;
    }

    glUseProgram(texturedShader_);

    const GLint vpLoc = glGetUniformLocation(texturedShader_, "uViewProjection");
    const GLint modelLoc = glGetUniformLocation(texturedShader_, "uModel");
    const GLint texLoc = glGetUniformLocation(texturedShader_, "uAlbedo");
    const GLint baseLoc = glGetUniformLocation(texturedShader_, "uBaseColor");

    glUniformMatrix4fv(vpLoc, 1, GL_FALSE, currentViewProjection_.data());
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, modelTransform.data());
    glUniform1i(texLoc, 0);

    glActiveTexture(GL_TEXTURE0);

    for (const GltfSubMesh& sub : gltfModels_[handle].subMeshes) {
        glUniform3f(baseLoc, sub.baseColor.x, sub.baseColor.y, sub.baseColor.z);
        glBindTexture(GL_TEXTURE_2D, sub.texture);
        glBindVertexArray(sub.vao);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(sub.indexCount), GL_UNSIGNED_INT, nullptr);
        ++stats_.drawCalls;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

bool Renderer::gltfMeshBounds(std::int32_t handle, Vec3& minOut, Vec3& maxOut) const {
    if (handle < 0 || static_cast<std::size_t>(handle) >= gltfModels_.size()) {
        return false;
    }
    minOut = gltfModels_[handle].aabbMin;
    maxOut = gltfModels_[handle].aabbMax;
    return true;
}

} // namespace Exo
