#pragma once

#include <string>
#include <vector>

#include <ExoEngine/Math/Vec.h>
#include <ExoEngine/Renderer/RenderEnvironment.h>
#include <ExoEngine/Renderer/RenderMaterialOverride.h>
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
    std::string visibleWhenFlag;
    std::string hiddenWhenFlag;
    std::string animationSet;
    std::string defaultClip;
    RenderMaterialOverride materialOverride {};
    Transform transform {};
    bool animationRig = false;
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
    void setRenderEnvironment(RenderEnvironment environment);

    [[nodiscard]] const std::string& name() const;
    [[nodiscard]] FixedCameraRig& cameraRig();
    [[nodiscard]] const FixedCameraRig& cameraRig() const;
    [[nodiscard]] const std::vector<StaticMeshInstance>& staticMeshes() const;
    [[nodiscard]] const std::vector<PointLight>& pointLights() const;
    [[nodiscard]] const RenderEnvironment& renderEnvironment() const;

    static Scene createReferenceScene();

private:
    std::string name_;
    FixedCameraRig cameraRig_;
    std::vector<StaticMeshInstance> staticMeshes_;
    std::vector<PointLight> pointLights_;
    RenderEnvironment renderEnvironment_ {};
};

} // namespace Exo
