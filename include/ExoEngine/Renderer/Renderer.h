#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <ExoEngine/Math/Mat4.h>
#include <ExoEngine/Math/Vec.h>
#include <ExoEngine/Renderer/MeshBuffer.h>
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

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool initialize(std::uint32_t width, std::uint32_t height);
    void shutdown();
    void resize(std::uint32_t width, std::uint32_t height);
    void beginFrame(const RenderView& view);
    [[nodiscard]] std::int32_t loadSceneMesh(const std::filesystem::path& path);
    void drawSceneMesh(std::int32_t handle, const Mat4& modelTransform);
    [[nodiscard]] bool sceneMeshBounds(std::int32_t handle, Vec3& minOut, Vec3& maxOut) const;
    void endFrame();

    [[nodiscard]] const RenderStats& stats() const;

private:
    bool createTexturedShader();

    struct GltfSubMesh {
        std::uint32_t vao = 0;
        std::uint32_t vbo = 0;
        std::uint32_t ebo = 0;
        std::uint32_t texture = 0;
        bool ownsTexture = false;
        std::uint32_t indexCount = 0;
        Vec3 baseColor {1.0f, 1.0f, 1.0f};
    };

    struct GltfModel {
        std::vector<GltfSubMesh> subMeshes;
        Vec3 aabbMin {0.0f, 0.0f, 0.0f};
        Vec3 aabbMax {0.0f, 0.0f, 0.0f};
    };

    struct ObjMaterialBinding {
        std::string name = "default";
        Vec3 baseColor {0.72f, 0.72f, 0.68f};
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
    void drawObjMesh(const SceneMesh& mesh);
    void drawGltfModel(const GltfModel& model);

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    Mat4 currentViewProjection_ = Mat4::identity();
    std::uint32_t texturedShader_ = 0;
    std::uint32_t whiteTexture_ = 0;
    std::vector<SceneMesh> sceneMeshes_;
    RenderStats stats_ {};
};

} // namespace Exo
