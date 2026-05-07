#pragma once

#include <string>
#include <vector>

namespace Exo {

struct MeshAsset;
class Scene;

enum class AssetValidationSeverity {
    Info,
    Warning,
    Error
};

enum class AssetValidationCode {
    EmptyMesh,
    InvalidBounds,
    MissingNormals,
    MissingTexCoords,
    NoSubmeshes,
    InvalidSubmeshIndexRange,
    MissingMaterialName,
    MissingSubmeshMaterialName,
    MissingStaticMeshAssetName,
    MissingStaticMeshMaterialName,
    NoCameras,
    InvalidLight,
    InvalidCamera,
    InvalidCameraBounds
};

struct AssetValidationDiagnostic {
    AssetValidationSeverity severity = AssetValidationSeverity::Info;
    AssetValidationCode code = AssetValidationCode::EmptyMesh;
    std::string path;
    std::string message;
};

struct AssetValidationReport {
    std::vector<AssetValidationDiagnostic> diagnostics;

    [[nodiscard]] bool hasErrors() const;
    [[nodiscard]] bool passed() const;
};

struct AssetValidationOptions {
    bool requireNormals = true;
    bool requireTexCoords = true;
    bool requireSubmeshMaterialNames = true;
    bool requireStaticMeshMaterialNames = true;
    bool requireCameras = true;
};

class AssetValidator {
public:
    explicit AssetValidator(AssetValidationOptions options = {});

    [[nodiscard]] AssetValidationReport validate(const MeshAsset& mesh) const;
    [[nodiscard]] AssetValidationReport validate(const Scene& scene) const;
    [[nodiscard]] AssetValidationReport validate(const MeshAsset& mesh, const Scene& scene) const;

private:
    AssetValidationOptions options_;
};

} // namespace Exo
