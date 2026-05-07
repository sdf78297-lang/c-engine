#pragma once

#include <string>
#include <vector>

#include <ExoEngine/Math/Vec.h>
#include <ExoEngine/Scene/FixedCameraRig.h>

namespace Exo {

struct Transform {
    Vec3 position {};
    Vec3 rotation {};
    Vec3 scale {1.0f, 1.0f, 1.0f};
};

struct StaticMeshInstance {
    std::string name;
    std::string meshAsset;
    std::string materialAsset;
    std::string meshSource;
    Transform transform {};
};

struct PointLight {
    Vec3 position {};
    Vec3 color {1.0f, 0.86f, 0.62f};
    float radius = 6.0f;
    float intensity = 1.0f;
};

class Scene {
public:
    explicit Scene(std::string name = "untitled");

    void addStaticMesh(StaticMeshInstance instance);
    void addPointLight(PointLight light);

    [[nodiscard]] const std::string& name() const;
    [[nodiscard]] FixedCameraRig& cameraRig();
    [[nodiscard]] const FixedCameraRig& cameraRig() const;
    [[nodiscard]] const std::vector<StaticMeshInstance>& staticMeshes() const;
    [[nodiscard]] const std::vector<PointLight>& pointLights() const;

    static Scene createReferenceScene();

private:
    std::string name_;
    FixedCameraRig cameraRig_;
    std::vector<StaticMeshInstance> staticMeshes_;
    std::vector<PointLight> pointLights_;
};

} // namespace Exo
