#pragma once

#include <ExoEngine/Math/Mat4.h>
#include <ExoEngine/Math/Vec.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Exo {

struct GltfVertex {
    Vec3 position {};
    Vec3 normal {0.0f, 1.0f, 0.0f};
    Vec2 uv {};
    std::array<std::uint16_t, 4> joints {0, 0, 0, 0};
    std::array<float, 4> weights {1.0f, 0.0f, 0.0f, 0.0f};
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
    std::int32_t skinIndex = -1;
    bool hasSkinning = false;
};

struct GltfNodeData {
    std::string name;
    std::int32_t parent = -1;
    std::vector<std::int32_t> children;
    Vec3 translation {};
    Quat rotation {};
    Vec3 scale {1.0f, 1.0f, 1.0f};
    Mat4 matrix = Mat4::identity();
    bool hasMatrix = false;
};

struct GltfSkinData {
    std::string name;
    std::vector<std::int32_t> joints;
    std::vector<Mat4> inverseBindMatrices;
    std::int32_t skeletonRoot = -1;
};

enum class GltfAnimationPath {
    Translation,
    Rotation,
    Scale,
    Weights,
    Unknown
};

struct GltfAnimationSamplerData {
    std::vector<float> times;
    std::vector<Vec4> values;
    std::string interpolation = "LINEAR";
};

struct GltfAnimationChannelData {
    std::int32_t samplerIndex = -1;
    std::int32_t targetNode = -1;
    GltfAnimationPath path = GltfAnimationPath::Unknown;
};

struct GltfAnimationClipData {
    std::string name;
    float duration = 0.0f;
    std::vector<GltfAnimationSamplerData> samplers;
    std::vector<GltfAnimationChannelData> channels;
};

struct GltfModelData {
    std::vector<GltfPrimitiveData> primitives;
    std::vector<GltfNodeData> nodes;
    std::vector<GltfSkinData> skins;
    std::vector<GltfAnimationClipData> animations;
    std::vector<std::string> morphTargetNames;
    Vec3 aabbMin {0.0f, 0.0f, 0.0f};
    Vec3 aabbMax {0.0f, 0.0f, 0.0f};
};

class GltfLoader {
public:
    [[nodiscard]] static GltfModelData loadFromFile(const std::filesystem::path& path);
};

} // namespace Exo
