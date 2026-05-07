#include <ExoEngine/Assets/GltfLoader.h>

#include <ExoEngine/Core/Logger.h>

#include <cgltf.h>
#include <stb_image.h>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace Exo {

namespace {

const cgltf_accessor* findAttribute(const cgltf_primitive& prim, cgltf_attribute_type type, std::int32_t setIndex = 0) {
    for (cgltf_size i = 0; i < prim.attributes_count; ++i) {
        const cgltf_attribute& attr = prim.attributes[i];
        if (attr.type == type && attr.index == setIndex) {
            return attr.data;
        }
    }
    return nullptr;
}

void decodeEmbeddedImage(const cgltf_image* image, GltfPrimitiveData& target) {
    if (image == nullptr) {
        return;
    }

    const std::uint8_t* data = nullptr;
    cgltf_size size = 0;

    if (image->buffer_view != nullptr) {
        data = static_cast<const std::uint8_t*>(image->buffer_view->buffer->data) + image->buffer_view->offset;
        size = image->buffer_view->size;
    } else {
        Logger::warn("GLB image has no embedded buffer view; external URIs are not supported yet");
        return;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* pixels = stbi_load_from_memory(
        data,
        static_cast<int>(size),
        &width,
        &height,
        &channels,
        STBI_rgb_alpha);

    if (pixels == nullptr) {
        Logger::warn(std::string("GLB embedded image decode failed: ") + stbi_failure_reason());
        return;
    }

    target.textureWidth = width;
    target.textureHeight = height;
    target.textureRgba.assign(pixels, pixels + (static_cast<std::size_t>(width) * height * 4));
    stbi_image_free(pixels);
}

} // namespace

GltfModelData GltfLoader::loadFromFile(const std::filesystem::path& path) {
    cgltf_options options {};
    cgltf_data* data = nullptr;

    const std::string pathString = path.string();
    cgltf_result parseResult = cgltf_parse_file(&options, pathString.c_str(), &data);
    if (parseResult != cgltf_result_success) {
        throw std::runtime_error("cgltf_parse_file failed for " + pathString);
    }

    cgltf_result loadResult = cgltf_load_buffers(&options, data, pathString.c_str());
    if (loadResult != cgltf_result_success) {
        cgltf_free(data);
        throw std::runtime_error("cgltf_load_buffers failed for " + pathString);
    }

    GltfModelData result;

    for (cgltf_size meshIndex = 0; meshIndex < data->meshes_count; ++meshIndex) {
        const cgltf_mesh& mesh = data->meshes[meshIndex];
        for (cgltf_size primIndex = 0; primIndex < mesh.primitives_count; ++primIndex) {
            const cgltf_primitive& prim = mesh.primitives[primIndex];
            if (prim.type != cgltf_primitive_type_triangles) {
                continue;
            }

            const cgltf_accessor* posAcc = findAttribute(prim, cgltf_attribute_type_position);
            if (posAcc == nullptr) {
                continue;
            }

            const cgltf_accessor* normAcc = findAttribute(prim, cgltf_attribute_type_normal);
            const cgltf_accessor* uvAcc = findAttribute(prim, cgltf_attribute_type_texcoord, 0);

            GltfPrimitiveData primData;
            const cgltf_size vertexCount = posAcc->count;
            primData.vertices.resize(vertexCount);

            constexpr float kInf = std::numeric_limits<float>::infinity();
            Vec3 primMin {kInf, kInf, kInf};
            Vec3 primMax {-kInf, -kInf, -kInf};

            for (cgltf_size vi = 0; vi < vertexCount; ++vi) {
                float pos[3] {0.0f, 0.0f, 0.0f};
                cgltf_accessor_read_float(posAcc, vi, pos, 3);
                primData.vertices[vi].position = {pos[0], pos[1], pos[2]};
                if (pos[0] < primMin.x) primMin.x = pos[0];
                if (pos[1] < primMin.y) primMin.y = pos[1];
                if (pos[2] < primMin.z) primMin.z = pos[2];
                if (pos[0] > primMax.x) primMax.x = pos[0];
                if (pos[1] > primMax.y) primMax.y = pos[1];
                if (pos[2] > primMax.z) primMax.z = pos[2];

                if (normAcc != nullptr) {
                    float n[3] {0.0f, 1.0f, 0.0f};
                    cgltf_accessor_read_float(normAcc, vi, n, 3);
                    primData.vertices[vi].normal = {n[0], n[1], n[2]};
                }

                if (uvAcc != nullptr) {
                    float uv[2] {0.0f, 0.0f};
                    cgltf_accessor_read_float(uvAcc, vi, uv, 2);
                    primData.vertices[vi].uv = {uv[0], uv[1]};
                }
            }

            if (prim.indices != nullptr) {
                const cgltf_size indexCount = prim.indices->count;
                primData.indices.resize(indexCount);
                for (cgltf_size ii = 0; ii < indexCount; ++ii) {
                    primData.indices[ii] = static_cast<std::uint32_t>(cgltf_accessor_read_index(prim.indices, ii));
                }
            } else {
                primData.indices.resize(vertexCount);
                for (cgltf_size i = 0; i < vertexCount; ++i) {
                    primData.indices[i] = static_cast<std::uint32_t>(i);
                }
            }

            if (prim.material != nullptr && prim.material->has_pbr_metallic_roughness) {
                const cgltf_pbr_metallic_roughness& pbr = prim.material->pbr_metallic_roughness;
                primData.baseColorFactor = {
                    pbr.base_color_factor[0],
                    pbr.base_color_factor[1],
                    pbr.base_color_factor[2],
                };

                if (pbr.base_color_texture.texture != nullptr) {
                    decodeEmbeddedImage(pbr.base_color_texture.texture->image, primData);
                }
            }

            primData.aabbMin = primMin;
            primData.aabbMax = primMax;
            result.primitives.push_back(std::move(primData));
        }
    }

    constexpr float kInf = std::numeric_limits<float>::infinity();
    Vec3 modelMin {kInf, kInf, kInf};
    Vec3 modelMax {-kInf, -kInf, -kInf};
    for (const GltfPrimitiveData& p : result.primitives) {
        if (p.aabbMin.x < modelMin.x) modelMin.x = p.aabbMin.x;
        if (p.aabbMin.y < modelMin.y) modelMin.y = p.aabbMin.y;
        if (p.aabbMin.z < modelMin.z) modelMin.z = p.aabbMin.z;
        if (p.aabbMax.x > modelMax.x) modelMax.x = p.aabbMax.x;
        if (p.aabbMax.y > modelMax.y) modelMax.y = p.aabbMax.y;
        if (p.aabbMax.z > modelMax.z) modelMax.z = p.aabbMax.z;
    }
    if (!result.primitives.empty()) {
        result.aabbMin = modelMin;
        result.aabbMax = modelMax;
    }

    cgltf_free(data);
    return result;
}

} // namespace Exo
