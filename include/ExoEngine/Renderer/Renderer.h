#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <ExoEngine/Assets/GltfLoader.h>
#include <ExoEngine/Math/Mat4.h>
#include <ExoEngine/Math/Vec.h>
#include <ExoEngine/Renderer/MeshBuffer.h>
#include <ExoEngine/Renderer/RenderEnvironment.h>
#include <ExoEngine/Renderer/RenderMaterialOverride.h>
#include <ExoEngine/Renderer/Texture2D.h>

namespace Exo {

struct RenderStats {
    std::uint64_t frameIndex = 0;
    std::uint32_t drawCalls = 0;
};

struct RenderView {
    Mat4 view = Mat4::identity();
    Mat4 projection = Mat4::identity();
    Vec3 cameraPosition {};
};

struct RenderPointLight {
    Vec3 position {};
    Vec3 color {1.0f, 0.86f, 0.62f};
    float radius = 6.0f;
    float intensity = 1.0f;
};

struct ScreenOverlay {
    float blackFade = 0.0f;
    float noiseIntensity = 0.0f;
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool initialize(std::uint32_t width, std::uint32_t height);
    void shutdown();
    void resize(std::uint32_t width, std::uint32_t height);
    void setEnvironment(const RenderEnvironment& environment);
    void setPointLights(const std::vector<RenderPointLight>& lights);
    void setScreenOverlay(ScreenOverlay overlay);
    void beginFrame(const RenderView& view);
    [[nodiscard]] std::int32_t loadSceneMesh(const std::filesystem::path& path);
    void drawSceneMesh(
        std::int32_t handle,
        const Mat4& modelTransform,
        const RenderMaterialOverride& materialOverride = {},
        const std::vector<Mat4>* jointMatrices = nullptr);
    [[nodiscard]] bool sceneMeshBounds(std::int32_t handle, Vec3& minOut, Vec3& maxOut) const;
    void endFrame();

    [[nodiscard]] const RenderStats& stats() const;

private:
    bool createTexturedShader();
    bool createScreenOverlayResources();
    void drawScreenOverlay();

    struct GltfSubMesh {
        std::uint32_t vao = 0;
        std::uint32_t vbo = 0;
        std::uint32_t ebo = 0;
        std::uint32_t texture = 0;
        bool ownsTexture = false;
        std::uint32_t indexCount = 0;
        Vec3 baseColor {1.0f, 1.0f, 1.0f};
        std::int32_t skinIndex = -1;
        bool skinned = false;
        std::vector<GltfVertex> baseVertices;
        std::vector<GltfVertex> skinnedVertices;
    };

    struct GltfModel {
        std::vector<GltfSubMesh> subMeshes;
        Vec3 aabbMin {0.0f, 0.0f, 0.0f};
        Vec3 aabbMax {0.0f, 0.0f, 0.0f};
    };

    struct ObjMaterialBinding {
        std::string name = "default";
        Vec3 baseColor {0.72f, 0.72f, 0.68f};
        Vec3 specularColor {0.04f, 0.04f, 0.04f};
        float shininess = 16.0f;
        std::size_t albedoTextureIndex = static_cast<std::size_t>(-1);
    };

    struct ObjDrawRange {
        std::uint32_t indexOffset = 0;
        std::uint32_t indexCount = 0;
        std::size_t materialIndex = 0;
    };

    struct SceneMesh {
        enum class Kind {
            Obj,
            Gltf
        };

        Kind kind = Kind::Obj;
        MeshBuffer objBuffer;
        std::vector<Texture2D> objTextures;
        std::vector<ObjMaterialBinding> objMaterials;
        std::vector<ObjDrawRange> objDrawRanges;
        GltfModel gltfModel;
        Vec3 aabbMin {0.0f, 0.0f, 0.0f};
        Vec3 aabbMax {0.0f, 0.0f, 0.0f};
    };

    [[nodiscard]] std::int32_t loadObjMesh(const std::filesystem::path& path);
    [[nodiscard]] std::int32_t loadGltfMesh(const std::filesystem::path& path);
    void cacheTexturedShaderUniforms();
    void drawObjMesh(const SceneMesh& mesh);
    void drawGltfModel(GltfModel& model, const std::vector<Mat4>* jointMatrices);
    void updateCpuSkinnedSubMesh(GltfSubMesh& subMesh, const std::vector<Mat4>& jointMatrices);

    struct TexturedShaderUniforms {
        int viewProjection = -1;
        int model = -1;
        int albedo = -1;
        int baseColor = -1;
        int materialSpecularColor = -1;
        int materialShininess = -1;
        int colorTint = -1;
        int emissiveColor = -1;
        int emissiveIntensity = -1;
        int cameraPosition = -1;
        int viewportSize = -1;
        int ambientColor = -1;
        int ambientIntensity = -1;
        int keyLightDirection = -1;
        int keyLightColor = -1;
        int keyLightIntensity = -1;
        int fogColor = -1;
        int fogStart = -1;
        int fogDensity = -1;
        int exposure = -1;
        int contrast = -1;
        int saturation = -1;
        int vignetteStrength = -1;
        int useSkinning = -1;
        int jointMatrices = -1;
        int pointLightCount = -1;
        std::array<int, 8> pointLightPosition {};
        std::array<int, 8> pointLightColor {};
        std::array<int, 8> pointLightRadius {};
        std::array<int, 8> pointLightIntensity {};
    };

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    Mat4 currentViewProjection_ = Mat4::identity();
    Vec3 currentCameraPosition_ {};
    RenderEnvironment activeEnvironment_ {};
    std::uint32_t texturedShader_ = 0;
    TexturedShaderUniforms texturedUniforms_ {};
    std::uint32_t screenOverlayShader_ = 0;
    std::uint32_t screenOverlayVao_ = 0;
    std::uint32_t whiteTexture_ = 0;
    ScreenOverlay screenOverlay_ {};
    std::vector<SceneMesh> sceneMeshes_;
    std::unordered_map<std::string, std::int32_t> sceneMeshCache_;
    std::vector<RenderPointLight> activePointLights_;
    RenderStats stats_ {};
};

} // namespace Exo
