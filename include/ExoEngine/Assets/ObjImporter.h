#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <ExoEngine/Assets/MeshAsset.h>

namespace Exo {

enum class AssetDiagnosticSeverity {
    Info,
    Warning,
    Error
};

struct AssetDiagnostic {
    AssetDiagnosticSeverity severity = AssetDiagnosticSeverity::Info;
    std::string message;
    std::filesystem::path source;
    std::size_t line = 0;
};

struct ObjImportOptions {
    bool generateMissingNormals = true;
    bool flipV = false;
    float scale = 1.0f;
};

struct ObjImportResult {
    MeshAsset mesh;
    std::vector<AssetDiagnostic> diagnostics;

    [[nodiscard]] bool success() const;
};

class ObjImporter {
public:
    ObjImportResult importFile(const std::filesystem::path& path, const ObjImportOptions& options = {}) const;
};

} // namespace Exo
