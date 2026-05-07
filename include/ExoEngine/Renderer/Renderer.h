#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include <ExoEngine/Math/Mat4.h>
#include <ExoEngine/Math/Vec.h>

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
    void drawReferenceRoom();
    void drawCharacter(const Mat4& modelTransform);
    void drawTV(const Mat4& modelTransform);
    [[nodiscard]] std::int32_t loadGltfMesh(const std::filesystem::path& path);
    void drawGltfMesh(std::int32_t handle, const Mat4& modelTransform);
    [[nodiscard]] bool gltfMeshBounds(std::int32_t handle, Vec3& minOut, Vec3& maxOut) const;
    void endFrame();

    [[nodiscard]] const RenderStats& stats() const;

private:
    bool createReferenceGeometry();
    bool createReferenceShader();
    bool createCharacterGeometry();
    bool createTVGeometry();
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

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    Mat4 currentViewProjection_ = Mat4::identity();
    std::uint32_t shader_ = 0;
    std::uint32_t vao_ = 0;
    std::uint32_t vbo_ = 0;
    std::uint32_t ebo_ = 0;
    std::uint32_t characterVao_ = 0;
    std::uint32_t characterVbo_ = 0;
    std::uint32_t characterEbo_ = 0;
    std::uint32_t characterIndexCount_ = 0;
    std::uint32_t tvVao_ = 0;
    std::uint32_t tvVbo_ = 0;
    std::uint32_t tvEbo_ = 0;
    std::uint32_t tvIndexCount_ = 0;
    std::uint32_t texturedShader_ = 0;
    std::uint32_t whiteTexture_ = 0;
    std::vector<GltfModel> gltfModels_;
    RenderStats stats_ {};
};

} // namespace Exo
