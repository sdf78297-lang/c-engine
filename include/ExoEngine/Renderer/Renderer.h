#pragma once

#include <cstdint>

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
    void endFrame();

    [[nodiscard]] const RenderStats& stats() const;

private:
    bool createReferenceGeometry();
    bool createReferenceShader();

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    Mat4 currentViewProjection_ = Mat4::identity();
    std::uint32_t shader_ = 0;
    std::uint32_t vao_ = 0;
    std::uint32_t vbo_ = 0;
    std::uint32_t ebo_ = 0;
    RenderStats stats_ {};
};

} // namespace Exo
