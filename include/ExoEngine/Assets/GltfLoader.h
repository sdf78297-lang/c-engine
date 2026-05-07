#pragma once

#include <ExoEngine/Math/Vec.h>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace Exo {

struct GltfVertex {
    Vec3 position {};
    Vec3 normal {0.0f, 1.0f, 0.0f};
    Vec2 uv {};
};

struct GltfPrimitiveData {
    std::vector<GltfVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<std::uint8_t> textureRgba;
    std::int32_t textureWidth = 0;
    std::int32_t textureHeight = 0;
    Vec3 baseColorFactor {1.0f, 1.0f, 1.0f};
    Vec3 aabbMin {0.0f, 0.0f, 0.0f};
    Vec3 aabbMax {0.0f, 0.0f, 0.0f};
};

struct GltfModelData {
    std::vector<GltfPrimitiveData> primitives;
    Vec3 aabbMin {0.0f, 0.0f, 0.0f};
    Vec3 aabbMax {0.0f, 0.0f, 0.0f};
};

class GltfLoader {
public:
    [[nodiscard]] static GltfModelData loadFromFile(const std::filesystem::path& path);
};

} // namespace Exo
