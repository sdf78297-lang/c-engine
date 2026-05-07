#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <ExoEngine/Math/Vec.h>

namespace Exo {

struct MeshBounds {
    Vec3 min {};
    Vec3 max {};
    bool valid = false;

    void include(Vec3 point);
};

struct MaterialTextureSet {
    std::filesystem::path albedo;
    std::filesystem::path normal;
    std::filesystem::path specular;
    std::filesystem::path opacity;
};

struct MaterialAsset {
    std::string name = "default";
    Vec3 ambient {0.02f, 0.02f, 0.02f};
    Vec3 diffuse {0.8f, 0.8f, 0.8f};
    Vec3 specular {0.04f, 0.04f, 0.04f};
    Vec3 emissive {0.0f, 0.0f, 0.0f};
    float shininess = 16.0f;
    float opacity = 1.0f;
    int illuminationModel = 2;
    MaterialTextureSet textures {};
};

struct MeshVertex {
    Vec3 position {};
    Vec3 normal {};
    Vec2 texCoord {};
    bool hasNormal = false;
    bool hasTexCoord = false;
};

struct MeshSubmesh {
    std::string name = "default";
    std::string materialName = "default";
    std::uint32_t indexOffset = 0;
    std::uint32_t indexCount = 0;
    MeshBounds bounds {};
};

struct MeshAsset {
    std::string name;
    std::filesystem::path sourcePath;
    std::vector<Vec3> sourcePositions;
    std::vector<Vec3> sourceNormals;
    std::vector<Vec2> sourceTexCoords;
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<MeshSubmesh> submeshes;
    std::vector<MaterialAsset> materials;
    MeshBounds bounds {};

    [[nodiscard]] bool empty() const;
};

} // namespace Exo
