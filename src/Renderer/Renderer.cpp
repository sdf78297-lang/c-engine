#include <ExoEngine/Renderer/Renderer.h>

#include <ExoEngine/Assets/GltfLoader.h>
#include <ExoEngine/Assets/ObjImporter.h>
#include <ExoEngine/Core/Logger.h>

#include <GL/glew.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace Exo {

namespace {

constexpr const char* kTexturedVertexShader = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec2 aFlags;

uniform mat4 uViewProjection;
uniform mat4 uModel;

out vec3 vNormalWS;
out vec3 vWorldPos;
out vec2 vUV;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vec3 normal = aFlags.x > 0.5 ? aNormal : vec3(0.0, 1.0, 0.0);
    vNormalWS = normalize(mat3(uModel) * normal);
    vWorldPos = worldPos.xyz;
    vUV = aFlags.y > 0.5 ? aUV : vec2(0.0);
    gl_Position = uViewProjection * worldPos;
}
)glsl";

constexpr const char* kTexturedFragmentShader = R"glsl(
#version 330 core
in vec3 vNormalWS;
in vec3 vWorldPos;
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uAlbedo;
uniform vec3 uBaseColor;
uniform int uPointLightCount;
uniform vec3 uPointLightPosition[8];
uniform vec3 uPointLightColor[8];
uniform float uPointLightRadius[8];
uniform float uPointLightIntensity[8];

void main() {
    vec4 sampled = texture(uAlbedo, vUV);
    vec3 albedo = sampled.rgb * uBaseColor;
    vec3 normal = normalize(vNormalWS);

    vec3 lightDir = normalize(vec3(0.40, 0.85, 0.32));
    float ndotl = max(dot(normal, lightDir), 0.0);
    float ambient = 0.38;
    vec3 lit = albedo * (ambient + ndotl * 0.58);

    for (int i = 0; i < 8; ++i) {
        if (i >= uPointLightCount) {
            break;
        }

        vec3 toLight = uPointLightPosition[i] - vWorldPos;
        float distanceToLight = length(toLight);
        float radius = max(uPointLightRadius[i], 0.001);
        float attenuation = clamp(1.0 - (distanceToLight / radius), 0.0, 1.0);
        attenuation *= attenuation;

        vec3 lightVector = distanceToLight > 0.001 ? toLight / distanceToLight : vec3(0.0, 1.0, 0.0);
        float pointDiffuse = max(dot(normal, lightVector), 0.0);
        vec3 pointLight = uPointLightColor[i] * uPointLightIntensity[i] * attenuation * (0.20 + pointDiffuse * 0.85);
        lit += albedo * pointLight;
    }

    FragColor = vec4(min(lit, vec3(1.0)), sampled.a);
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

std::string lowerExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext;
}

std::string meshCacheKey(const std::filesystem::path& path) {
    return std::filesystem::absolute(path).lexically_normal().generic_string();
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

    if (!createTexturedShader()) {
        shutdown();
        return false;
    }

    Logger::info("OpenGL renderer initialized");
    return true;
}

void Renderer::shutdown() {
    for (SceneMesh& mesh : sceneMeshes_) {
        mesh.objBuffer.destroy();
        for (Texture2D& texture : mesh.objTextures) {
            texture.destroy();
        }
        for (GltfSubMesh& sub : mesh.gltfModel.subMeshes) {
            if (sub.ebo != 0) {
                glDeleteBuffers(1, &sub.ebo);
                sub.ebo = 0;
            }
            if (sub.vbo != 0) {
                glDeleteBuffers(1, &sub.vbo);
                sub.vbo = 0;
            }
            if (sub.vao != 0) {
                glDeleteVertexArrays(1, &sub.vao);
                sub.vao = 0;
            }
            if (sub.texture != 0 && sub.ownsTexture) {
                glDeleteTextures(1, &sub.texture);
                sub.texture = 0;
            }
        }
        mesh.gltfModel.subMeshes.clear();
    }
    sceneMeshes_.clear();
    sceneMeshCache_.clear();
    activePointLights_.clear();

    if (whiteTexture_ != 0) {
        glDeleteTextures(1, &whiteTexture_);
        whiteTexture_ = 0;
    }

    if (texturedShader_ != 0) {
        glDeleteProgram(texturedShader_);
        texturedShader_ = 0;
    }
}

void Renderer::resize(std::uint32_t width, std::uint32_t height) {
    width_ = width;
    height_ = height == 0 ? 1 : height;
    glViewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
}

void Renderer::setPointLights(const std::vector<RenderPointLight>& lights) {
    activePointLights_ = lights;
    if (activePointLights_.size() > 8) {
        activePointLights_.resize(8);
    }
}

void Renderer::beginFrame(const RenderView& view) {
    stats_.drawCalls = 0;
    ++stats_.frameIndex;
    currentViewProjection_ = view.projection * view.view;

    glClearColor(0.018f, 0.021f, 0.023f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

std::int32_t Renderer::loadSceneMesh(const std::filesystem::path& path) {
    const std::string cacheKey = meshCacheKey(path);
    if (const auto it = sceneMeshCache_.find(cacheKey); it != sceneMeshCache_.end()) {
        return it->second;
    }

    const std::string ext = lowerExtension(path);
    std::int32_t handle = -1;
    if (ext == ".obj") {
        handle = loadObjMesh(path);
    } else if (ext == ".glb" || ext == ".gltf") {
        handle = loadGltfMesh(path);
    } else {
        Logger::error("Unsupported scene mesh format: " + path.string());
        return -1;
    }

    if (handle >= 0) {
        sceneMeshCache_.emplace(cacheKey, handle);
    }
    return handle;
}

void Renderer::drawSceneMesh(std::int32_t handle, const Mat4& modelTransform) {
    if (handle < 0 || static_cast<std::size_t>(handle) >= sceneMeshes_.size() || texturedShader_ == 0) {
        return;
    }

    glUseProgram(texturedShader_);

    const GLint vpLoc = glGetUniformLocation(texturedShader_, "uViewProjection");
    const GLint modelLoc = glGetUniformLocation(texturedShader_, "uModel");
    const GLint texLoc = glGetUniformLocation(texturedShader_, "uAlbedo");
    const GLint pointCountLoc = glGetUniformLocation(texturedShader_, "uPointLightCount");

    glUniformMatrix4fv(vpLoc, 1, GL_FALSE, currentViewProjection_.data());
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, modelTransform.data());
    glUniform1i(texLoc, 0);
    glUniform1i(pointCountLoc, static_cast<GLint>(activePointLights_.size()));
    glActiveTexture(GL_TEXTURE0);

    for (std::size_t i = 0; i < activePointLights_.size(); ++i) {
        const RenderPointLight& light = activePointLights_[i];
        const std::string index = std::to_string(i);
        glUniform3f(glGetUniformLocation(texturedShader_, ("uPointLightPosition[" + index + "]").c_str()),
            light.position.x, light.position.y, light.position.z);
        glUniform3f(glGetUniformLocation(texturedShader_, ("uPointLightColor[" + index + "]").c_str()),
            light.color.x, light.color.y, light.color.z);
        glUniform1f(glGetUniformLocation(texturedShader_, ("uPointLightRadius[" + index + "]").c_str()), light.radius);
        glUniform1f(glGetUniformLocation(texturedShader_, ("uPointLightIntensity[" + index + "]").c_str()), light.intensity);
    }

    SceneMesh& mesh = sceneMeshes_[handle];
    if (mesh.kind == SceneMesh::Kind::Obj) {
        drawObjMesh(mesh);
    } else {
        drawGltfModel(mesh.gltfModel);
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

bool Renderer::sceneMeshBounds(std::int32_t handle, Vec3& minOut, Vec3& maxOut) const {
    if (handle < 0 || static_cast<std::size_t>(handle) >= sceneMeshes_.size()) {
        return false;
    }
    minOut = sceneMeshes_[handle].aabbMin;
    maxOut = sceneMeshes_[handle].aabbMax;
    return true;
}

void Renderer::endFrame() {}

const RenderStats& Renderer::stats() const {
    return stats_;
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

std::int32_t Renderer::loadObjMesh(const std::filesystem::path& path) {
    ObjImportResult result = ObjImporter {}.importFile(path);
    for (const AssetDiagnostic& diagnostic : result.diagnostics) {
        const std::string line = diagnostic.line == 0 ? "" : ":" + std::to_string(diagnostic.line);
        const std::string message = diagnostic.source.string() + line + " " + diagnostic.message;
        if (diagnostic.severity == AssetDiagnosticSeverity::Error) {
            Logger::error(message);
        } else if (diagnostic.severity == AssetDiagnosticSeverity::Warning) {
            Logger::warn(message);
        } else {
            Logger::info(message);
        }
    }

    if (!result.success()) {
        Logger::error("OBJ load failed: " + path.string());
        return -1;
    }

    SceneMesh mesh;
    mesh.kind = SceneMesh::Kind::Obj;
    if (!mesh.objBuffer.upload(result.mesh)) {
        return -1;
    }

    mesh.objMaterials.reserve(result.mesh.materials.size() + 1);
    mesh.objMaterials.push_back({});

    std::unordered_map<std::string, std::size_t> materialIndices;
    std::unordered_map<std::string, std::size_t> textureIndices;
    materialIndices.emplace(mesh.objMaterials.front().name, 0);

    for (const MaterialAsset& material : result.mesh.materials) {
        ObjMaterialBinding binding;
        binding.name = material.name.empty() ? "default" : material.name;
        binding.baseColor = material.diffuse;

        if (!material.textures.albedo.empty()) {
            const std::string textureKey = material.textures.albedo.lexically_normal().string();
            if (const auto it = textureIndices.find(textureKey); it != textureIndices.end()) {
                binding.albedoTextureIndex = it->second;
            } else {
                Texture2D albedo;
                TextureLoadOptions textureOptions;
                textureOptions.flipVertically = false;
                textureOptions.generateMipmaps = true;
                textureOptions.srgb = true;
                if (albedo.loadFromFile(material.textures.albedo, textureOptions)) {
                    binding.albedoTextureIndex = mesh.objTextures.size();
                    textureIndices.emplace(textureKey, binding.albedoTextureIndex);
                    mesh.objTextures.push_back(std::move(albedo));
                } else {
                    Logger::warn("OBJ material albedo fallback: " + binding.name);
                }
            }
        }

        const std::size_t materialIndex = mesh.objMaterials.size();
        materialIndices[binding.name] = materialIndex;
        mesh.objMaterials.push_back(std::move(binding));
    }

    mesh.objDrawRanges.reserve(result.mesh.submeshes.size());
    for (const MeshSubmesh& submesh : result.mesh.submeshes) {
        if (submesh.indexCount == 0) {
            continue;
        }

        std::size_t materialIndex = 0;
        if (const auto it = materialIndices.find(submesh.materialName); it != materialIndices.end()) {
            materialIndex = it->second;
        } else {
            Logger::warn("OBJ submesh uses missing material '" + submesh.materialName + "': " + path.filename().string());
        }

        mesh.objDrawRanges.push_back({
            .indexOffset = submesh.indexOffset,
            .indexCount = submesh.indexCount,
            .materialIndex = materialIndex,
        });
    }

    if (mesh.objDrawRanges.empty() && result.mesh.indices.size() <= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        mesh.objDrawRanges.push_back({
            .indexOffset = 0,
            .indexCount = static_cast<std::uint32_t>(result.mesh.indices.size()),
            .materialIndex = 0,
        });
    }

    mesh.aabbMin = result.mesh.bounds.valid ? result.mesh.bounds.min : Vec3 {};
    mesh.aabbMax = result.mesh.bounds.valid ? result.mesh.bounds.max : Vec3 {};

    sceneMeshes_.push_back(std::move(mesh));
    Logger::info("Scene OBJ loaded: " + path.filename().string()
        + " (" + std::to_string(result.mesh.vertices.size()) + " verts, "
        + std::to_string(result.mesh.indices.size() / 3) + " tris)");
    return static_cast<std::int32_t>(sceneMeshes_.size() - 1);
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

    SceneMesh sceneMesh;
    sceneMesh.kind = SceneMesh::Kind::Gltf;
    sceneMesh.aabbMin = data.aabbMin;
    sceneMesh.aabbMax = data.aabbMax;
    sceneMesh.gltfModel.aabbMin = data.aabbMin;
    sceneMesh.gltfModel.aabbMax = data.aabbMax;
    sceneMesh.gltfModel.subMeshes.reserve(data.primitives.size());

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
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GltfVertex), reinterpret_cast<void*>(offsetof(GltfVertex, uv)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GltfVertex), reinterpret_cast<void*>(offsetof(GltfVertex, normal)));
        glDisableVertexAttribArray(3);
        glVertexAttrib2f(3, 1.0f, 1.0f);

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
                GL_SRGB8_ALPHA8,
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
        sceneMesh.gltfModel.subMeshes.push_back(sub);
    }

    sceneMeshes_.push_back(std::move(sceneMesh));
    Logger::info("Scene GLB loaded: " + path.filename().string()
        + " (" + std::to_string(data.primitives.size()) + " prims, "
        + std::to_string(totalVerts) + " verts, "
        + std::to_string(totalTris) + " tris)");
    return static_cast<std::int32_t>(sceneMeshes_.size() - 1);
}

void Renderer::drawObjMesh(const SceneMesh& mesh) {
    const GLint baseLoc = glGetUniformLocation(texturedShader_, "uBaseColor");

    for (const ObjDrawRange& range : mesh.objDrawRanges) {
        const ObjMaterialBinding& material = range.materialIndex < mesh.objMaterials.size()
            ? mesh.objMaterials[range.materialIndex]
            : mesh.objMaterials.front();

        glUniform3f(baseLoc, material.baseColor.x, material.baseColor.y, material.baseColor.z);
        if (material.albedoTextureIndex < mesh.objTextures.size() && mesh.objTextures[material.albedoTextureIndex].valid()) {
            mesh.objTextures[material.albedoTextureIndex].bind(0);
        } else {
            glBindTexture(GL_TEXTURE_2D, whiteTexture_);
        }

        mesh.objBuffer.drawRange(range.indexOffset, range.indexCount);
        ++stats_.drawCalls;
    }
}

void Renderer::drawGltfModel(const GltfModel& model) {
    const GLint baseLoc = glGetUniformLocation(texturedShader_, "uBaseColor");

    for (const GltfSubMesh& sub : model.subMeshes) {
        glUniform3f(baseLoc, sub.baseColor.x, sub.baseColor.y, sub.baseColor.z);
        glBindTexture(GL_TEXTURE_2D, sub.texture);
        glBindVertexArray(sub.vao);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(sub.indexCount), GL_UNSIGNED_INT, nullptr);
        ++stats_.drawCalls;
    }
}

} // namespace Exo
