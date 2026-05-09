#include <ExoEngine/Scene/SceneLoader.h>

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace Exo {
namespace {

using Json = nlohmann::json;

constexpr float degreesToRadians(float degrees) {
    return degrees * 0.017453292519943295769f;
}

std::string childPath(std::string_view parent, std::string_view key) {
    std::string path(parent);
    path += ".";
    path += key;
    return path;
}

std::string childPath(std::string_view parent, std::size_t index) {
    std::string path(parent);
    path += "[";
    path += std::to_string(index);
    path += "]";
    return path;
}

class SceneJsonReader {
public:
    SceneJsonReader(const Json& root, std::string_view sourceName)
        : root_(root), sourceName_(sourceName) {}

    [[nodiscard]] Scene read() const {
        expectObject(root_, "$");

        Scene scene(requiredStringField(root_, "name", "$"));
        readStaticMeshes(scene);
        readPointLights(scene);
        readFixedCameras(scene);

        return scene;
    }

private:
    [[noreturn]] void fail(std::string_view path, std::string_view message) const {
        throw std::runtime_error(
            "Scene JSON error in " + sourceName_ + " at " + std::string(path) + ": " + std::string(message));
    }

    void expectObject(const Json& value, std::string_view path) const {
        if (!value.is_object()) {
            fail(path, "expected object");
        }
    }

    void expectArray(const Json& value, std::string_view path) const {
        if (!value.is_array()) {
            fail(path, "expected array");
        }
    }

    [[nodiscard]] const Json* optionalField(const Json& object, std::string_view field) const {
        const auto it = object.find(std::string(field));
        if (it == object.end()) {
            return nullptr;
        }

        return &(*it);
    }

    [[nodiscard]] const Json& requiredField(const Json& object, std::string_view field, std::string_view path) const {
        const Json* value = optionalField(object, field);
        if (value == nullptr) {
            fail(childPath(path, field), "missing required field");
        }

        return *value;
    }

    [[nodiscard]] std::string requiredStringField(
        const Json& object,
        std::string_view field,
        std::string_view path) const {
        const std::string fieldPath = childPath(path, field);
        const Json& value = requiredField(object, field, path);
        if (!value.is_string()) {
            fail(fieldPath, "expected string");
        }

        std::string result = value.get<std::string>();
        if (result.empty()) {
            fail(fieldPath, "must not be empty");
        }

        return result;
    }

    [[nodiscard]] std::string optionalStringField(
        const Json& object,
        std::string_view field,
        std::string_view path,
        std::string fallback) const {
        const Json* value = optionalField(object, field);
        if (value == nullptr) {
            return fallback;
        }

        const std::string fieldPath = childPath(path, field);
        if (!value->is_string()) {
            fail(fieldPath, "expected string");
        }

        return value->get<std::string>();
    }

    [[nodiscard]] float numberValue(const Json& value, std::string_view path) const {
        if (!value.is_number()) {
            fail(path, "expected number");
        }

        const double rawValue = value.get<double>();
        if (!std::isfinite(rawValue)) {
            fail(path, "must be finite");
        }

        return static_cast<float>(rawValue);
    }

    [[nodiscard]] float requiredNumberField(
        const Json& object,
        std::string_view field,
        std::string_view path) const {
        const std::string fieldPath = childPath(path, field);
        return numberValue(requiredField(object, field, path), fieldPath);
    }

    [[nodiscard]] float optionalNumberField(
        const Json& object,
        std::string_view field,
        std::string_view path,
        float fallback) const {
        const Json* value = optionalField(object, field);
        if (value == nullptr) {
            return fallback;
        }

        return numberValue(*value, childPath(path, field));
    }

    [[nodiscard]] std::int32_t requiredInt32Field(
        const Json& object,
        std::string_view field,
        std::string_view path) const {
        const std::string fieldPath = childPath(path, field);
        const Json& value = requiredField(object, field, path);

        std::int64_t rawValue = 0;
        if (value.is_number_unsigned()) {
            const std::uint64_t unsignedValue = value.get<std::uint64_t>();
            if (unsignedValue > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
                fail(fieldPath, "outside int32 range");
            }

            rawValue = static_cast<std::int64_t>(unsignedValue);
        } else if (value.is_number_integer()) {
            rawValue = value.get<std::int64_t>();
        } else {
            fail(fieldPath, "expected integer");
        }

        if (rawValue < std::numeric_limits<std::int32_t>::min()
            || rawValue > std::numeric_limits<std::int32_t>::max()) {
            fail(fieldPath, "outside int32 range");
        }

        return static_cast<std::int32_t>(rawValue);
    }

    [[nodiscard]] Vec3 vec3Value(const Json& value, std::string_view path) const {
        expectArray(value, path);
        if (value.size() != 3) {
            fail(path, "expected exactly 3 numeric values");
        }

        return {
            numberValue(value[0], childPath(path, 0)),
            numberValue(value[1], childPath(path, 1)),
            numberValue(value[2], childPath(path, 2)),
        };
    }

    [[nodiscard]] Vec3 requiredVec3Field(const Json& object, std::string_view field, std::string_view path) const {
        const std::string fieldPath = childPath(path, field);
        return vec3Value(requiredField(object, field, path), fieldPath);
    }

    [[nodiscard]] Vec3 optionalVec3Field(
        const Json& object,
        std::string_view field,
        std::string_view path,
        Vec3 fallback) const {
        const Json* value = optionalField(object, field);
        if (value == nullptr) {
            return fallback;
        }

        return vec3Value(*value, childPath(path, field));
    }

    [[nodiscard]] Bounds3 boundsValue(const Json& value, std::string_view path) const {
        expectObject(value, path);

        Bounds3 bounds;
        bounds.min = requiredVec3Field(value, "min", path);
        bounds.max = requiredVec3Field(value, "max", path);

        if (bounds.min.x > bounds.max.x || bounds.min.y > bounds.max.y || bounds.min.z > bounds.max.z) {
            fail(path, "min must be less than or equal to max on every axis");
        }

        return bounds;
    }

    [[nodiscard]] Bounds3 requiredBoundsField(const Json& object, std::string_view field, std::string_view path) const {
        const std::string fieldPath = childPath(path, field);
        return boundsValue(requiredField(object, field, path), fieldPath);
    }

    void validatePositive(Vec3 value, std::string_view path) const {
        if (value.x <= 0.0f || value.y <= 0.0f || value.z <= 0.0f) {
            fail(path, "all components must be greater than zero");
        }
    }

    void validateNonNegative(Vec3 value, std::string_view path) const {
        if (value.x < 0.0f || value.y < 0.0f || value.z < 0.0f) {
            fail(path, "all components must be greater than or equal to zero");
        }
    }

    [[nodiscard]] Transform transformValue(const Json& value, std::string_view path) const {
        expectObject(value, path);

        Transform transform;
        transform.position = optionalVec3Field(value, "position", path, transform.position);
        transform.rotation = optionalVec3Field(value, "rotation", path, transform.rotation);
        transform.scale = optionalVec3Field(value, "scale", path, transform.scale);
        validatePositive(transform.scale, childPath(path, "scale"));

        return transform;
    }

    void readStaticMeshes(Scene& scene) const {
        const Json* meshes = optionalField(root_, "staticMeshes");
        if (meshes == nullptr) {
            return;
        }

        expectArray(*meshes, "$.staticMeshes");
        for (std::size_t index = 0; index < meshes->size(); ++index) {
            const std::string meshPath = childPath("$.staticMeshes", index);
            const Json& mesh = (*meshes)[index];
            expectObject(mesh, meshPath);

            StaticMeshInstance instance;
            instance.name = requiredStringField(mesh, "name", meshPath);
            instance.meshAsset = requiredStringField(mesh, "meshAsset", meshPath);
            instance.materialAsset = optionalStringField(mesh, "materialAsset", meshPath, "");
            instance.meshSource = optionalStringField(mesh, "meshSource", meshPath, "");
            instance.visibleWhenFlag = optionalStringField(mesh, "visibleWhenFlag", meshPath, "");
            instance.hiddenWhenFlag = optionalStringField(mesh, "hiddenWhenFlag", meshPath, "");

            if (const Json* transform = optionalField(mesh, "transform")) {
                instance.transform = transformValue(*transform, childPath(meshPath, "transform"));
            }

            scene.addStaticMesh(std::move(instance));
        }
    }

    void readPointLights(Scene& scene) const {
        const Json* lights = optionalField(root_, "pointLights");
        if (lights == nullptr) {
            return;
        }

        expectArray(*lights, "$.pointLights");
        for (std::size_t index = 0; index < lights->size(); ++index) {
            const std::string lightPath = childPath("$.pointLights", index);
            const Json& lightJson = (*lights)[index];
            expectObject(lightJson, lightPath);

            PointLight light;
            light.position = requiredVec3Field(lightJson, "position", lightPath);
            light.color = optionalVec3Field(lightJson, "color", lightPath, light.color);
            validateNonNegative(light.color, childPath(lightPath, "color"));

            light.radius = optionalNumberField(lightJson, "radius", lightPath, light.radius);
            if (light.radius <= 0.0f) {
                fail(childPath(lightPath, "radius"), "must be greater than zero");
            }

            light.intensity = optionalNumberField(lightJson, "intensity", lightPath, light.intensity);
            if (light.intensity < 0.0f) {
                fail(childPath(lightPath, "intensity"), "must be greater than or equal to zero");
            }

            scene.addPointLight(light);
        }
    }

    void readFixedCameras(Scene& scene) const {
        const Json* cameras = optionalField(root_, "fixedCameras");
        if (cameras == nullptr) {
            return;
        }

        expectArray(*cameras, "$.fixedCameras");
        for (std::size_t index = 0; index < cameras->size(); ++index) {
            const std::string cameraPath = childPath("$.fixedCameras", index);
            const Json& camera = (*cameras)[index];
            expectObject(camera, cameraPath);

            FixedCameraShot shot;
            shot.name = requiredStringField(camera, "name", cameraPath);
            shot.activationBounds = requiredBoundsField(camera, "bounds", cameraPath);
            shot.position = requiredVec3Field(camera, "position", cameraPath);
            shot.target = requiredVec3Field(camera, "target", cameraPath);

            const float fovDeg = requiredNumberField(camera, "fovDeg", cameraPath);
            if (fovDeg <= 1.0f || fovDeg >= 179.0f) {
                fail(childPath(cameraPath, "fovDeg"), "must be greater than 1 and less than 179");
            }
            shot.fovRadians = degreesToRadians(fovDeg);

            shot.nearPlane = requiredNumberField(camera, "near", cameraPath);
            if (shot.nearPlane <= 0.0f) {
                fail(childPath(cameraPath, "near"), "must be greater than zero");
            }

            shot.farPlane = requiredNumberField(camera, "far", cameraPath);
            if (shot.farPlane <= shot.nearPlane) {
                fail(childPath(cameraPath, "far"), "must be greater than near");
            }

            shot.priority = requiredInt32Field(camera, "priority", cameraPath);

            shot.blendSeconds = requiredNumberField(camera, "blendSeconds", cameraPath);
            if (shot.blendSeconds < 0.0f) {
                fail(childPath(cameraPath, "blendSeconds"), "must be greater than or equal to zero");
            }

            scene.cameraRig().addShot(std::move(shot));
        }
    }

    const Json& root_;
    std::string sourceName_;
};

} // namespace

Scene SceneLoader::loadFromFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input) {
        throw std::runtime_error("Unable to open scene JSON file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    return loadFromJsonString(buffer.str(), path.string());
}

Scene SceneLoader::loadFromJsonString(std::string_view source, std::string_view sourceName) {
    Json root;
    try {
        root = Json::parse(source.begin(), source.end());
    } catch (const Json::parse_error& error) {
        throw std::runtime_error("Scene JSON parse error in " + std::string(sourceName) + ": " + error.what());
    }

    return SceneJsonReader(root, sourceName).read();
}

} // namespace Exo
