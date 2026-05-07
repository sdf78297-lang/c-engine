#include <ExoEngine/Assets/AssetValidator.h>

#include <ExoEngine/Assets/MeshAsset.h>
#include <ExoEngine/Scene/Scene.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Exo {
namespace {

constexpr float minimumCameraFovRadians = 0.017453292519943295769f;
constexpr float maximumCameraFovRadians = 3.12413936106985f;

bool finite(float value) {
    return std::isfinite(value);
}

bool finite(Vec2 value) {
    return finite(value.x) && finite(value.y);
}

bool finite(Vec3 value) {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

bool validRange(Vec3 min, Vec3 max) {
    return finite(min) && finite(max)
        && min.x <= max.x
        && min.y <= max.y
        && min.z <= max.z;
}

bool validBounds(const MeshBounds& bounds) {
    return bounds.valid && validRange(bounds.min, bounds.max);
}

bool validBounds(const Bounds3& bounds) {
    return validRange(bounds.min, bounds.max);
}

std::string indexedPath(std::string_view base, std::size_t index) {
    std::string path(base);
    path += "[";
    path += std::to_string(index);
    path += "]";
    return path;
}

std::string fieldPath(std::string path, std::string_view field) {
    path += ".";
    path += field;
    return path;
}

void addDiagnostic(
    AssetValidationReport& report,
    AssetValidationSeverity severity,
    AssetValidationCode code,
    std::string path,
    std::string message
) {
    report.diagnostics.push_back({
        .severity = severity,
        .code = code,
        .path = std::move(path),
        .message = std::move(message),
    });
}

void validateMeshBounds(AssetValidationReport& report, const MeshAsset& mesh) {
    if (!validBounds(mesh.bounds)) {
        addDiagnostic(
            report,
            AssetValidationSeverity::Error,
            AssetValidationCode::InvalidBounds,
            "mesh.bounds",
            "Mesh bounds are missing, inverted or contain non-finite values");
    }
}

void validateVertexStreams(AssetValidationReport& report, const MeshAsset& mesh, const AssetValidationOptions& options) {
    bool missingNormal = false;
    bool missingTexCoord = false;
    bool invalidPosition = false;
    bool invalidNormal = false;
    bool invalidTexCoord = false;

    for (const MeshVertex& vertex : mesh.vertices) {
        missingNormal = missingNormal || !vertex.hasNormal;
        missingTexCoord = missingTexCoord || !vertex.hasTexCoord;
        invalidPosition = invalidPosition || !finite(vertex.position);
        invalidNormal = invalidNormal || (vertex.hasNormal && !finite(vertex.normal));
        invalidTexCoord = invalidTexCoord || (vertex.hasTexCoord && !finite(vertex.texCoord));
    }

    if (invalidPosition || invalidNormal || invalidTexCoord) {
        addDiagnostic(
            report,
            AssetValidationSeverity::Error,
            AssetValidationCode::InvalidBounds,
            "mesh.vertices",
            "Mesh vertex streams contain non-finite values");
    }

    if (options.requireNormals && missingNormal) {
        addDiagnostic(
            report,
            AssetValidationSeverity::Warning,
            AssetValidationCode::MissingNormals,
            "mesh.vertices",
            "Mesh has vertices without authored or generated normals");
    }

    if (options.requireTexCoords && missingTexCoord) {
        addDiagnostic(
            report,
            AssetValidationSeverity::Warning,
            AssetValidationCode::MissingTexCoords,
            "mesh.vertices",
            "Mesh has vertices without UV coordinates");
    }
}

void validateMaterials(AssetValidationReport& report, const MeshAsset& mesh) {
    for (std::size_t index = 0; index < mesh.materials.size(); ++index) {
        if (mesh.materials[index].name.empty()) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Warning,
                AssetValidationCode::MissingMaterialName,
                fieldPath(indexedPath("mesh.materials", index), "name"),
                "Material asset has no name and cannot be referenced reliably");
        }
    }
}

void validateSubmeshes(AssetValidationReport& report, const MeshAsset& mesh, const AssetValidationOptions& options) {
    if (mesh.submeshes.empty()) {
        addDiagnostic(
            report,
            AssetValidationSeverity::Error,
            AssetValidationCode::NoSubmeshes,
            "mesh.submeshes",
            "Mesh has no submeshes to bind geometry and materials");
        return;
    }

    const std::uint64_t indexBufferSize = static_cast<std::uint64_t>(mesh.indices.size());
    const std::uint64_t vertexBufferSize = static_cast<std::uint64_t>(mesh.vertices.size());

    for (std::size_t submeshIndex = 0; submeshIndex < mesh.submeshes.size(); ++submeshIndex) {
        const MeshSubmesh& submesh = mesh.submeshes[submeshIndex];
        const std::string submeshPath = indexedPath("mesh.submeshes", submeshIndex);
        const std::uint64_t offset = submesh.indexOffset;
        const std::uint64_t count = submesh.indexCount;
        const std::uint64_t end = offset + count;
        const bool rangeOverflowed = end < offset;

        if (count == 0 || (count % 3u) != 0u || rangeOverflowed || offset > indexBufferSize || end > indexBufferSize) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Error,
                AssetValidationCode::InvalidSubmeshIndexRange,
                submeshPath,
                "Submesh index range is empty, not triangle-aligned or outside the mesh index buffer");
            continue;
        }

        for (std::uint64_t indexOffset = offset; indexOffset < end; ++indexOffset) {
            const std::uint32_t vertexIndex = mesh.indices[static_cast<std::size_t>(indexOffset)];
            if (static_cast<std::uint64_t>(vertexIndex) >= vertexBufferSize) {
                addDiagnostic(
                    report,
                    AssetValidationSeverity::Error,
                    AssetValidationCode::InvalidSubmeshIndexRange,
                    fieldPath(submeshPath, "indices"),
                    "Submesh references a vertex outside the mesh vertex buffer");
                break;
            }
        }

        if (!validBounds(submesh.bounds)) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Error,
                AssetValidationCode::InvalidBounds,
                fieldPath(submeshPath, "bounds"),
                "Submesh bounds are missing, inverted or contain non-finite values");
        }

        if (options.requireSubmeshMaterialNames && submesh.materialName.empty()) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Warning,
                AssetValidationCode::MissingSubmeshMaterialName,
                fieldPath(submeshPath, "materialName"),
                "Submesh has no material name");
        }
    }
}

void validateStaticMeshes(AssetValidationReport& report, const Scene& scene, const AssetValidationOptions& options) {
    const std::vector<StaticMeshInstance>& meshes = scene.staticMeshes();
    for (std::size_t index = 0; index < meshes.size(); ++index) {
        const StaticMeshInstance& mesh = meshes[index];
        const std::string meshPath = indexedPath("scene.staticMeshes", index);

        if (mesh.meshAsset.empty()) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Error,
                AssetValidationCode::MissingStaticMeshAssetName,
                fieldPath(meshPath, "meshAsset"),
                "Static mesh instance does not name a mesh asset");
        }

        if (options.requireStaticMeshMaterialNames && mesh.materialAsset.empty()) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Warning,
                AssetValidationCode::MissingStaticMeshMaterialName,
                fieldPath(meshPath, "materialAsset"),
                "Static mesh instance does not name a material asset");
        }
    }
}

void validateLights(AssetValidationReport& report, const Scene& scene) {
    const std::vector<PointLight>& lights = scene.pointLights();
    for (std::size_t index = 0; index < lights.size(); ++index) {
        const PointLight& light = lights[index];
        const std::string lightPath = indexedPath("scene.pointLights", index);

        if (!finite(light.position)) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Error,
                AssetValidationCode::InvalidLight,
                fieldPath(lightPath, "position"),
                "Point light position contains non-finite values");
        }

        if (!finite(light.color) || light.color.x < 0.0f || light.color.y < 0.0f || light.color.z < 0.0f) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Error,
                AssetValidationCode::InvalidLight,
                fieldPath(lightPath, "color"),
                "Point light color must be finite and non-negative");
        }

        if (!finite(light.radius) || light.radius <= 0.0f) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Error,
                AssetValidationCode::InvalidLight,
                fieldPath(lightPath, "radius"),
                "Point light radius must be finite and greater than zero");
        }

        if (!finite(light.intensity) || light.intensity < 0.0f) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Error,
                AssetValidationCode::InvalidLight,
                fieldPath(lightPath, "intensity"),
                "Point light intensity must be finite and non-negative");
        }
    }
}

void validateCameras(AssetValidationReport& report, const Scene& scene, const AssetValidationOptions& options) {
    const FixedCameraRig& rig = scene.cameraRig();
    const std::vector<FixedCameraShot>& shots = rig.shots();

    if (options.requireCameras && shots.empty()) {
        addDiagnostic(
            report,
            AssetValidationSeverity::Error,
            AssetValidationCode::NoCameras,
            "scene.cameraRig",
            "Scene has no fixed cameras");
        return;
    }

    for (std::size_t index = 0; index < shots.size(); ++index) {
        const FixedCameraShot& shot = shots[index];
        const std::string shotPath = indexedPath("scene.fixedCameras", index);

        if (!validBounds(shot.activationBounds)) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Error,
                AssetValidationCode::InvalidCameraBounds,
                fieldPath(shotPath, "bounds"),
                "Camera activation bounds are inverted or contain non-finite values");
        }

        if (!finite(shot.position) || !finite(shot.target)) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Error,
                AssetValidationCode::InvalidCamera,
                shotPath,
                "Camera position and target must contain finite values");
        }

        if (!finite(shot.fovRadians)
            || shot.fovRadians <= minimumCameraFovRadians
            || shot.fovRadians >= maximumCameraFovRadians) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Error,
                AssetValidationCode::InvalidCamera,
                fieldPath(shotPath, "fovRadians"),
                "Camera FOV must be finite and stay between 1 and 179 degrees");
        }

        if (!finite(shot.nearPlane) || shot.nearPlane <= 0.0f) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Error,
                AssetValidationCode::InvalidCamera,
                fieldPath(shotPath, "nearPlane"),
                "Camera near plane must be finite and greater than zero");
        }

        if (!finite(shot.farPlane) || shot.farPlane <= shot.nearPlane) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Error,
                AssetValidationCode::InvalidCamera,
                fieldPath(shotPath, "farPlane"),
                "Camera far plane must be finite and greater than near plane");
        }

        if (!finite(shot.blendSeconds) || shot.blendSeconds < 0.0f) {
            addDiagnostic(
                report,
                AssetValidationSeverity::Error,
                AssetValidationCode::InvalidCamera,
                fieldPath(shotPath, "blendSeconds"),
                "Camera blend time must be finite and non-negative");
        }
    }
}

} // namespace

bool AssetValidationReport::hasErrors() const {
    return std::ranges::any_of(diagnostics, [](const AssetValidationDiagnostic& diagnostic) {
        return diagnostic.severity == AssetValidationSeverity::Error;
    });
}

bool AssetValidationReport::passed() const {
    return !hasErrors();
}

AssetValidator::AssetValidator(AssetValidationOptions options)
    : options_(options) {}

AssetValidationReport AssetValidator::validate(const MeshAsset& mesh) const {
    AssetValidationReport report;

    if (mesh.empty()) {
        addDiagnostic(
            report,
            AssetValidationSeverity::Error,
            AssetValidationCode::EmptyMesh,
            "mesh",
            "Mesh has no renderable vertex or index data");
    }

    validateMeshBounds(report, mesh);
    validateVertexStreams(report, mesh, options_);
    validateMaterials(report, mesh);
    validateSubmeshes(report, mesh, options_);

    return report;
}

AssetValidationReport AssetValidator::validate(const Scene& scene) const {
    AssetValidationReport report;

    validateStaticMeshes(report, scene, options_);
    validateLights(report, scene);
    validateCameras(report, scene, options_);

    return report;
}

AssetValidationReport AssetValidator::validate(const MeshAsset& mesh, const Scene& scene) const {
    AssetValidationReport report = validate(mesh);
    AssetValidationReport sceneReport = validate(scene);

    report.diagnostics.insert(
        report.diagnostics.end(),
        std::make_move_iterator(sceneReport.diagnostics.begin()),
        std::make_move_iterator(sceneReport.diagnostics.end()));

    return report;
}

} // namespace Exo
