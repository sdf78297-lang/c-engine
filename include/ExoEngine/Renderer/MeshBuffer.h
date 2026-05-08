#pragma once

#include <cstdint>
#include <string>

#include <ExoEngine/Assets/MeshAsset.h>

namespace Exo {

class MeshBuffer {
public:
    MeshBuffer() = default;
    ~MeshBuffer();

    MeshBuffer(const MeshBuffer&) = delete;
    MeshBuffer& operator=(const MeshBuffer&) = delete;

    MeshBuffer(MeshBuffer&& other) noexcept;
    MeshBuffer& operator=(MeshBuffer&& other) noexcept;

    [[nodiscard]] bool upload(const MeshAsset& mesh);
    void destroy();
    void draw() const;
    void drawRange(std::uint32_t indexOffset, std::uint32_t indexCount) const;

    [[nodiscard]] bool valid() const;
    [[nodiscard]] std::uint32_t indexCount() const;
    [[nodiscard]] const MeshBounds& bounds() const;
    [[nodiscard]] const std::string& sourceName() const;

private:
    std::uint32_t vao_ = 0;
    std::uint32_t vbo_ = 0;
    std::uint32_t ebo_ = 0;
    std::uint32_t indexCount_ = 0;
    MeshBounds bounds_ {};
    std::string sourceName_;
};

} // namespace Exo
