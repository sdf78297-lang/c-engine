#include <ExoEngine/Assets/ObjImporter.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace Exo {

namespace {

struct ObjIndex {
    int position = 0;
    int texCoord = 0;
    int normal = 0;

    [[nodiscard]] bool operator==(const ObjIndex& other) const {
        return position == other.position && texCoord == other.texCoord && normal == other.normal;
    }
};

struct ObjIndexHash {
    std::size_t operator()(const ObjIndex& index) const {
        const std::size_t p = static_cast<std::size_t>(index.position) * 73856093u;
        const std::size_t t = static_cast<std::size_t>(index.texCoord) * 19349663u;
        const std::size_t n = static_cast<std::size_t>(index.normal) * 83492791u;
        return p ^ t ^ n;
    }
};

std::string trim(std::string_view view) {
    const auto begin = view.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) {
        return {};
    }

    const auto end = view.find_last_not_of(" \t\r\n");
    return std::string(view.substr(begin, end - begin + 1));
}

std::string stripComment(std::string_view line) {
    const auto comment = line.find('#');
    if (comment == std::string_view::npos) {
        return trim(line);
    }
    return trim(line.substr(0, comment));
}

bool parseFloat(std::string_view token, float& out) {
    const char* begin = token.data();
    const char* end = token.data() + token.size();
    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc {} && result.ptr == end;
}

bool parseInt(std::string_view token, int& out) {
    const char* begin = token.data();
    const char* end = token.data() + token.size();
    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc {} && result.ptr == end;
}

std::vector<std::string> splitTokens(std::string_view line) {
    std::vector<std::string> result;
    std::istringstream stream {std::string(line)};
    std::string token;
    while (stream >> token) {
        result.push_back(token);
    }
    return result;
}

std::string restAfterKeyword(std::string_view line) {
    const auto firstSpace = line.find_first_of(" \t");
    if (firstSpace == std::string_view::npos) {
        return {};
    }
    return trim(line.substr(firstSpace + 1));
}

std::filesystem::path texturePathFromLine(std::string_view line) {
    std::string rest = restAfterKeyword(line);
    const auto optionEnd = rest.find_last_of(" \t");
    if (rest.starts_with("-") && optionEnd != std::string::npos) {
        return trim(rest.substr(optionEnd + 1));
    }
    return rest;
}

std::filesystem::path resolveTexturePath(const std::filesystem::path& materialPath, std::string_view line) {
    const std::filesystem::path texturePath = texturePathFromLine(line);
    if (texturePath.empty() || texturePath.is_absolute()) {
        return texturePath;
    }
    return materialPath.parent_path() / texturePath;
}

void addDiagnostic(
    std::vector<AssetDiagnostic>& diagnostics,
    AssetDiagnosticSeverity severity,
    std::filesystem::path source,
    std::size_t line,
    std::string message
) {
    diagnostics.push_back({
        .severity = severity,
        .message = std::move(message),
        .source = std::move(source),
        .line = line,
    });
}

int resolveObjIndex(int rawIndex, std::size_t count) {
    if (rawIndex > 0) {
        return rawIndex - 1;
    }

    if (rawIndex < 0) {
        return static_cast<int>(count) + rawIndex;
    }

    return -1;
}

ObjIndex parseObjIndex(std::string_view token) {
    ObjIndex result;
    std::array<std::string_view, 3> parts {};
    std::size_t part = 0;
    std::size_t start = 0;

    while (part < parts.size()) {
        const auto slash = token.find('/', start);
        parts[part++] = token.substr(start, slash == std::string_view::npos ? token.size() - start : slash - start);
        if (slash == std::string_view::npos) {
            break;
        }
        start = slash + 1;
    }

    if (!parts[0].empty()) {
        parseInt(parts[0], result.position);
    }

    if (!parts[1].empty()) {
        parseInt(parts[1], result.texCoord);
    }

    if (!parts[2].empty()) {
        parseInt(parts[2], result.normal);
    }

    return result;
}

Vec3 faceNormal(const MeshVertex& a, const MeshVertex& b, const MeshVertex& c) {
    return normalize(cross(b.position - a.position, c.position - a.position));
}

std::vector<MaterialAsset> loadMtl(
    const std::filesystem::path& path,
    std::vector<AssetDiagnostic>& diagnostics
) {
    std::ifstream file(path);
    if (!file) {
        addDiagnostic(diagnostics, AssetDiagnosticSeverity::Warning, path, 0, "MTL file could not be opened");
        return {};
    }

    std::vector<MaterialAsset> materials;
    std::unordered_map<std::string, bool> materialNames;
    MaterialAsset* current = nullptr;
    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        const std::string clean = stripComment(line);
        if (clean.empty()) {
            continue;
        }

        const std::vector<std::string> tokens = splitTokens(clean);
        if (tokens.empty()) {
            continue;
        }

        const std::string& keyword = tokens.front();

        if (keyword == "newmtl") {
            MaterialAsset material;
            material.name = restAfterKeyword(clean);
            if (material.name.empty()) {
                material.name = "unnamed_material";
                addDiagnostic(diagnostics, AssetDiagnosticSeverity::Warning, path, lineNumber, "Material has no name");
            }
            if (materialNames.contains(material.name)) {
                addDiagnostic(diagnostics, AssetDiagnosticSeverity::Warning, path, lineNumber, "Duplicate material '" + material.name + "' is ignored");
                current = nullptr;
                continue;
            }
            materialNames.emplace(material.name, true);
            materials.push_back(std::move(material));
            current = &materials.back();
            continue;
        }

        if (current == nullptr) {
            addDiagnostic(diagnostics, AssetDiagnosticSeverity::Warning, path, lineNumber, "MTL property appears before newmtl");
            continue;
        }

        auto parseColor = [&](Vec3& color) {
            if (tokens.size() < 4) {
                addDiagnostic(diagnostics, AssetDiagnosticSeverity::Warning, path, lineNumber, "Color property needs three components");
                return;
            }
            parseFloat(tokens[1], color.x);
            parseFloat(tokens[2], color.y);
            parseFloat(tokens[3], color.z);
        };

        if (keyword == "Ka") {
            parseColor(current->ambient);
        } else if (keyword == "Kd") {
            parseColor(current->diffuse);
        } else if (keyword == "Ks") {
            parseColor(current->specular);
        } else if (keyword == "Ke") {
            parseColor(current->emissive);
        } else if (keyword == "Ns" && tokens.size() >= 2) {
            parseFloat(tokens[1], current->shininess);
        } else if ((keyword == "d" || keyword == "Tr") && tokens.size() >= 2) {
            parseFloat(tokens[1], current->opacity);
            if (keyword == "Tr") {
                current->opacity = 1.0f - current->opacity;
            }
        } else if (keyword == "illum" && tokens.size() >= 2) {
            parseInt(tokens[1], current->illuminationModel);
        } else if (keyword == "map_Kd") {
            current->textures.albedo = resolveTexturePath(path, clean);
        } else if (keyword == "map_Ks") {
            current->textures.specular = resolveTexturePath(path, clean);
        } else if (keyword == "map_d") {
            current->textures.opacity = resolveTexturePath(path, clean);
        } else if (keyword == "map_Bump" || keyword == "bump") {
            current->textures.normal = resolveTexturePath(path, clean);
        }
    }

    return materials;
}

} // namespace

bool ObjImportResult::success() const {
    return !mesh.empty()
        && std::ranges::none_of(diagnostics, [](const AssetDiagnostic& diagnostic) {
            return diagnostic.severity == AssetDiagnosticSeverity::Error;
        });
}

ObjImportResult ObjImporter::importFile(const std::filesystem::path& path, const ObjImportOptions& options) const {
    ObjImportResult result;
    result.mesh.sourcePath = path;
    result.mesh.name = path.stem().string();

    std::ifstream file(path);
    if (!file) {
        addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Error, path, 0, "OBJ file could not be opened");
        return result;
    }

    std::vector<Vec3> positions;
    std::vector<Vec2> texCoords;
    std::vector<Vec3> normals;
    std::unordered_map<ObjIndex, std::uint32_t, ObjIndexHash> vertexCache;

    std::string currentObject = result.mesh.name.empty() ? "default" : result.mesh.name;
    std::string currentMaterial = "default";
    bool sawMissingNormals = false;
    bool sawMissingTexCoords = false;
    bool sawMaterialReference = false;
    bool sawUseMtl = false;
    bool sawMtlLib = false;

    auto beginSubmesh = [&]() -> MeshSubmesh& {
        if (result.mesh.submeshes.empty()
            || result.mesh.submeshes.back().materialName != currentMaterial
            || result.mesh.submeshes.back().name != currentObject) {
            result.mesh.submeshes.push_back({
                .name = currentObject,
                .materialName = currentMaterial,
                .indexOffset = static_cast<std::uint32_t>(result.mesh.indices.size()),
                .indexCount = 0,
            });
        }
        return result.mesh.submeshes.back();
    };

    auto isResolvedIndexValid = [](int index, std::size_t count) {
        return index >= 0 && static_cast<std::size_t>(index) < count;
    };

    auto resolveFaceIndex = [&](ObjIndex raw, std::size_t lineNumber, ObjIndex& resolved) {
        resolved.position = resolveObjIndex(raw.position, positions.size());
        if (!isResolvedIndexValid(resolved.position, positions.size())) {
            addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Error, path, lineNumber, "Face references an invalid position index");
            return false;
        }

        resolved.texCoord = -1;
        if (raw.texCoord != 0) {
            resolved.texCoord = resolveObjIndex(raw.texCoord, texCoords.size());
            if (!isResolvedIndexValid(resolved.texCoord, texCoords.size())) {
                addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Error, path, lineNumber, "Face references an invalid texcoord index");
                return false;
            }
        } else {
            sawMissingTexCoords = true;
        }

        resolved.normal = -1;
        if (raw.normal != 0) {
            resolved.normal = resolveObjIndex(raw.normal, normals.size());
            if (!isResolvedIndexValid(resolved.normal, normals.size())) {
                addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Error, path, lineNumber, "Face references an invalid normal index");
                return false;
            }
        } else {
            sawMissingNormals = true;
        }

        return true;
    };

    auto makeVertex = [&](ObjIndex resolved, std::size_t lineNumber) -> std::uint32_t {
        if (auto it = vertexCache.find(resolved); it != vertexCache.end()) {
            return it->second;
        }

        if (result.mesh.vertices.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Error, path, lineNumber, "OBJ exceeds 32-bit vertex index range");
            return 0;
        }

        MeshVertex vertex;
        vertex.position = positions[static_cast<std::size_t>(resolved.position)] * options.scale;
        result.mesh.bounds.include(vertex.position);

        if (resolved.texCoord >= 0) {
            vertex.texCoord = texCoords[static_cast<std::size_t>(resolved.texCoord)];
            if (options.flipV) {
                vertex.texCoord.y = 1.0f - vertex.texCoord.y;
            }
            vertex.hasTexCoord = true;
        }

        if (resolved.normal >= 0) {
            vertex.normal = normals[static_cast<std::size_t>(resolved.normal)];
            vertex.hasNormal = true;
        }

        const std::uint32_t newIndex = static_cast<std::uint32_t>(result.mesh.vertices.size());
        result.mesh.vertices.push_back(vertex);
        vertexCache.emplace(resolved, newIndex);
        return newIndex;
    };

    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        const std::string clean = stripComment(line);
        if (clean.empty()) {
            continue;
        }

        const std::vector<std::string> tokens = splitTokens(clean);
        if (tokens.empty()) {
            continue;
        }

        const std::string& keyword = tokens.front();

        if (keyword == "v") {
            if (tokens.size() < 4) {
                addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Warning, path, lineNumber, "Position needs three components");
                continue;
            }

            Vec3 position;
            parseFloat(tokens[1], position.x);
            parseFloat(tokens[2], position.y);
            parseFloat(tokens[3], position.z);
            positions.push_back(position);
        } else if (keyword == "vt") {
            if (tokens.size() < 3) {
                addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Warning, path, lineNumber, "Texcoord needs two components");
                continue;
            }

            Vec2 texCoord;
            parseFloat(tokens[1], texCoord.x);
            parseFloat(tokens[2], texCoord.y);
            texCoords.push_back(texCoord);
        } else if (keyword == "vn") {
            if (tokens.size() < 4) {
                addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Warning, path, lineNumber, "Normal needs three components");
                continue;
            }

            Vec3 normal;
            parseFloat(tokens[1], normal.x);
            parseFloat(tokens[2], normal.y);
            parseFloat(tokens[3], normal.z);
            normals.push_back(normalize(normal));
        } else if (keyword == "f") {
            if (tokens.size() < 4) {
                addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Warning, path, lineNumber, "Face needs at least three vertices");
                continue;
            }

            std::vector<ObjIndex> resolvedPolygon;
            resolvedPolygon.reserve(tokens.size() - 1);
            bool validFace = true;
            for (std::size_t i = 1; i < tokens.size(); ++i) {
                ObjIndex resolved;
                if (!resolveFaceIndex(parseObjIndex(tokens[i]), lineNumber, resolved)) {
                    validFace = false;
                    break;
                }
                resolvedPolygon.push_back(resolved);
            }

            if (!validFace) {
                continue;
            }

            std::vector<std::uint32_t> polygon;
            polygon.reserve(resolvedPolygon.size());
            for (const ObjIndex& resolved : resolvedPolygon) {
                polygon.push_back(makeVertex(resolved, lineNumber));
            }

            MeshSubmesh& submesh = beginSubmesh();
            for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
                const std::array<std::uint32_t, 3> triangle {
                    polygon[0],
                    polygon[i],
                    polygon[i + 1],
                };

                if (options.generateMissingNormals) {
                    MeshVertex& a = result.mesh.vertices[triangle[0]];
                    MeshVertex& b = result.mesh.vertices[triangle[1]];
                    MeshVertex& c = result.mesh.vertices[triangle[2]];
                    const Vec3 normal = faceNormal(a, b, c);
                    if (!a.hasNormal) {
                        a.normal = a.normal + normal;
                    }
                    if (!b.hasNormal) {
                        b.normal = b.normal + normal;
                    }
                    if (!c.hasNormal) {
                        c.normal = c.normal + normal;
                    }
                }

                result.mesh.indices.insert(result.mesh.indices.end(), triangle.begin(), triangle.end());
                submesh.indexCount += 3;
                submesh.bounds.include(result.mesh.vertices[triangle[0]].position);
                submesh.bounds.include(result.mesh.vertices[triangle[1]].position);
                submesh.bounds.include(result.mesh.vertices[triangle[2]].position);
            }
        } else if (keyword == "o" || keyword == "g") {
            const std::string name = restAfterKeyword(clean);
            if (!name.empty()) {
                currentObject = name;
            }
        } else if (keyword == "usemtl") {
            currentMaterial = restAfterKeyword(clean);
            if (currentMaterial.empty()) {
                currentMaterial = "default";
                addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Warning, path, lineNumber, "usemtl has no material name");
            }
            sawMaterialReference = true;
            sawUseMtl = true;
        } else if (keyword == "mtllib") {
            sawMaterialReference = true;
            sawMtlLib = true;
            if (tokens.size() < 2) {
                addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Warning, path, lineNumber, "mtllib has no file name");
                continue;
            }

            for (std::size_t i = 1; i < tokens.size(); ++i) {
                const std::filesystem::path mtlPath = path.parent_path() / tokens[i];
                std::vector<MaterialAsset> materials = loadMtl(mtlPath, result.diagnostics);
                result.mesh.materials.insert(
                    result.mesh.materials.end(),
                    std::make_move_iterator(materials.begin()),
                    std::make_move_iterator(materials.end())
                );
            }
        }
    }

    result.mesh.sourcePositions = std::move(positions);
    result.mesh.sourceNormals = std::move(normals);
    result.mesh.sourceTexCoords = std::move(texCoords);

    if (!result.mesh.indices.empty() && !sawUseMtl) {
        addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Warning, path, 0, "OBJ geometry has no usemtl assignments; using fallback material");
    }

    if (!result.mesh.indices.empty() && !sawMtlLib) {
        addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Warning, path, 0, "OBJ geometry has no mtllib statement; material data is unavailable");
    }

    if (result.mesh.materials.empty()) {
        result.mesh.materials.push_back({});
        if (sawMaterialReference) {
            addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Warning, path, 0, "No valid MTL materials were loaded; using fallback material");
        }
    }

    if (options.generateMissingNormals) {
        for (MeshVertex& vertex : result.mesh.vertices) {
            if (!vertex.hasNormal && vertex.normal.length() > 0.00001f) {
                vertex.normal = normalize(vertex.normal);
                vertex.hasNormal = true;
            }
        }
    }

    auto hasMaterial = [&](const std::string& materialName) {
        return std::ranges::any_of(result.mesh.materials, [&](const MaterialAsset& material) {
            return material.name == materialName;
        });
    };

    for (const MeshSubmesh& submesh : result.mesh.submeshes) {
        if (sawUseMtl && !hasMaterial(submesh.materialName)) {
            addDiagnostic(
                result.diagnostics,
                AssetDiagnosticSeverity::Warning,
                path,
                0,
                "Material '" + submesh.materialName + "' was referenced but not found in loaded MTL files");
        }
    }

    if (sawMissingNormals) {
        const char* message = options.generateMissingNormals
            ? "Some faces have no normals; generated face normals where possible"
            : "Some faces have no normals";
        addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Warning, path, 0, message);
    }

    if (sawMissingTexCoords) {
        addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Warning, path, 0, "Some faces have no UVs; texture quality depends on authored UVs");
    }

    if (result.mesh.indices.empty()) {
        addDiagnostic(result.diagnostics, AssetDiagnosticSeverity::Error, path, 0, "OBJ contains no renderable faces");
    }

    return result;
}

} // namespace Exo
