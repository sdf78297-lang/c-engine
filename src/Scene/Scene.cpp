#include <ExoEngine/Scene/Scene.h>

#include <utility>

namespace Exo {

Scene::Scene(std::string name)
    : name_(std::move(name)) {}

void Scene::addStaticMesh(StaticMeshInstance instance) {
    staticMeshes_.push_back(std::move(instance));
}

void Scene::addPointLight(PointLight light) {
    pointLights_.push_back(light);
}

const std::string& Scene::name() const {
    return name_;
}

FixedCameraRig& Scene::cameraRig() {
    return cameraRig_;
}

const FixedCameraRig& Scene::cameraRig() const {
    return cameraRig_;
}

const std::vector<StaticMeshInstance>& Scene::staticMeshes() const {
    return staticMeshes_;
}

const std::vector<PointLight>& Scene::pointLights() const {
    return pointLights_;
}

Scene Scene::createReferenceScene() {
    Scene scene("engine_reference_room");

    scene.addStaticMesh({
        .name = "reference_room",
        .meshAsset = "engine/reference_room.mesh",
        .materialAsset = "engine/reference_room.material",
    });

    scene.addPointLight({
        .position = {0.0f, 1.8f, 0.7f},
        .color = {1.0f, 0.78f, 0.52f},
        .radius = 5.5f,
        .intensity = 1.0f,
    });

    scene.cameraRig().addShot({
        .name = "entry_angle",
        .activationBounds = {{-3.0f, -0.5f, -3.0f}, {3.0f, 2.5f, 3.0f}},
        .position = {4.6f, 2.35f, 5.2f},
        .target = {0.0f, 0.85f, 0.0f},
        .fovRadians = 0.7853981f,
        .nearPlane = 0.05f,
        .farPlane = 80.0f,
        .priority = 10,
    });

    return scene;
}

} // namespace Exo
