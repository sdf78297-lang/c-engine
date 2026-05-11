#include <ExoEngine/Assets/GltfLoader.h>

#include <ExoEngine/Core/Logger.h>

#include <cgltf.h>
#include <stb_image.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>

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

std::int32_t nodeIndex(const cgltf_data* data, const cgltf_node* node) {
    if (data == nullptr || node == nullptr) {
        return -1;
    }
    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        if (&data->nodes[i] == node) {
            return static_cast<std::int32_t>(i);
        }
    }
    return -1;
}

std::int32_t skinIndex(const cgltf_data* data, const cgltf_skin* skin) {
    if (data == nullptr || skin == nullptr) {
        return -1;
    }
    for (cgltf_size i = 0; i < data->skins_count; ++i) {
        if (&data->skins[i] == skin) {
            return static_cast<std::int32_t>(i);
        }
    }
    return -1;
}

std::int32_t meshIndex(const cgltf_data* data, const cgltf_mesh* mesh) {
    if (data == nullptr || mesh == nullptr) {
        return -1;
    }
    for (cgltf_size i = 0; i < data->meshes_count; ++i) {
        if (&data->meshes[i] == mesh) {
            return static_cast<std::int32_t>(i);
        }
    }
    return -1;
}

Mat4 accessorMat4(const cgltf_accessor* accessor, cgltf_size index) {
    Mat4 result = Mat4::identity();
    if (accessor == nullptr) {
        return result;
    }

    float values[16] {};
    if (!cgltf_accessor_read_float(accessor, index, values, 16)) {
        return result;
    }
    for (int i = 0; i < 16; ++i) {
        result.values[static_cast<std::size_t>(i)] = values[i];
    }
    return result;
}

GltfAnimationPath animationPath(cgltf_animation_path_type path) {
    switch (path) {
    case cgltf_animation_path_type_translation:
        return GltfAnimationPath::Translation;
    case cgltf_animation_path_type_rotation:
        return GltfAnimationPath::Rotation;
    case cgltf_animation_path_type_scale:
        return GltfAnimationPath::Scale;
    case cgltf_animation_path_type_weights:
        return GltfAnimationPath::Weights;
    default:
        return GltfAnimationPath::Unknown;
    }
}

std::string interpolationName(cgltf_interpolation_type interpolation) {
    switch (interpolation) {
    case cgltf_interpolation_type_step:
        return "STEP";
    case cgltf_interpolation_type_cubic_spline:
        return "CUBICSPLINE";
    case cgltf_interpolation_type_linear:
    default:
        return "LINEAR";
    }
}

cgltf_size componentCount(const cgltf_accessor* accessor) {
    if (accessor == nullptr) {
        return 0;
    }
    switch (accessor->type) {
    case cgltf_type_scalar:
        return 1;
    case cgltf_type_vec2:
        return 2;
    case cgltf_type_vec3:
        return 3;
    case cgltf_type_vec4:
        return 4;
    case cgltf_type_mat4:
        return 16;
    default:
        return 0;
    }
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

void readNodes(const cgltf_data* data, GltfModelData& result) {
    result.nodes.resize(data->nodes_count);
    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        const cgltf_node& node = data->nodes[i];
        GltfNodeData& out = result.nodes[i];
        out.name = node.name != nullptr ? node.name : ("node_" + std::to_string(i));
        out.parent = nodeIndex(data, node.parent);
        out.children.reserve(node.children_count);
        for (cgltf_size childIndex = 0; childIndex < node.children_count; ++childIndex) {
            out.children.push_back(nodeIndex(data, node.children[childIndex]));
        }

        out.hasMatrix = node.has_matrix;
        if (node.has_matrix) {
            for (int m = 0; m < 16; ++m) {
                out.matrix.values[static_cast<std::size_t>(m)] = node.matrix[m];
            }
        }
        if (node.has_translation) {
            out.translation = {node.translation[0], node.translation[1], node.translation[2]};
        }
        if (node.has_rotation) {
            out.rotation = {node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]};
        }
        if (node.has_scale) {
            out.scale = {node.scale[0], node.scale[1], node.scale[2]};
        }
    }
}

void readSkins(const cgltf_data* data, GltfModelData& result) {
    result.skins.resize(data->skins_count);
    for (cgltf_size i = 0; i < data->skins_count; ++i) {
        const cgltf_skin& skin = data->skins[i];
        GltfSkinData& out = result.skins[i];
        out.name = skin.name != nullptr ? skin.name : ("skin_" + std::to_string(i));
        out.skeletonRoot = nodeIndex(data, skin.skeleton);
        out.joints.reserve(skin.joints_count);
        for (cgltf_size jointIndex = 0; jointIndex < skin.joints_count; ++jointIndex) {
            out.joints.push_back(nodeIndex(data, skin.joints[jointIndex]));
        }

        out.inverseBindMatrices.resize(out.joints.size(), Mat4::identity());
        if (skin.inverse_bind_matrices != nullptr) {
            const cgltf_size matrixCount = std::min(
                skin.inverse_bind_matrices->count,
                static_cast<cgltf_size>(out.inverseBindMatrices.size()));
            for (cgltf_size matrixIndex = 0; matrixIndex < matrixCount; ++matrixIndex) {
                out.inverseBindMatrices[matrixIndex] = accessorMat4(skin.inverse_bind_matrices, matrixIndex);
            }
        }
    }
}

std::vector<std::int32_t> meshSkinBindings(const cgltf_data* data) {
    std::vector<std::int32_t> bindings(data->meshes_count, -1);
    for (cgltf_size nodeIdx = 0; nodeIdx < data->nodes_count; ++nodeIdx) {
        const cgltf_node& node = data->nodes[nodeIdx];
        const std::int32_t meshIdx = meshIndex(data, node.mesh);
        const std::int32_t skinIdx = skinIndex(data, node.skin);
        if (meshIdx >= 0 && skinIdx >= 0 && static_cast<std::size_t>(meshIdx) < bindings.size()) {
            bindings[static_cast<std::size_t>(meshIdx)] = skinIdx;
        }
    }
    return bindings;
}

void readAnimations(const cgltf_data* data, GltfModelData& result) {
    result.animations.reserve(data->animations_count);
    for (cgltf_size animIndex = 0; animIndex < data->animations_count; ++animIndex) {
        const cgltf_animation& animation = data->animations[animIndex];
        GltfAnimationClipData clip;
        clip.name = animation.name != nullptr ? animation.name : ("animation_" + std::to_string(animIndex));
        clip.samplers.resize(animation.samplers_count);
        clip.channels.resize(animation.channels_count);

        for (cgltf_size samplerIndex = 0; samplerIndex < animation.samplers_count; ++samplerIndex) {
            const cgltf_animation_sampler& sampler = animation.samplers[samplerIndex];
            GltfAnimationSamplerData& out = clip.samplers[samplerIndex];
            out.interpolation = interpolationName(sampler.interpolation);

            if (sampler.input != nullptr) {
                out.times.resize(sampler.input->count);
                for (cgltf_size i = 0; i < sampler.input->count; ++i) {
                    float value = 0.0f;
                    cgltf_accessor_read_float(sampler.input, i, &value, 1);
                    out.times[i] = value;
                    clip.duration = std::max(clip.duration, value);
                }
            }

            if (sampler.output != nullptr && !out.times.empty()) {
                const cgltf_size components = std::min<cgltf_size>(componentCount(sampler.output), 4);
                const bool cubic = sampler.interpolation == cgltf_interpolation_type_cubic_spline
                    && sampler.output->count >= out.times.size() * 3;
                out.values.resize(out.times.size());
                for (cgltf_size i = 0; i < out.times.size(); ++i) {
                    const cgltf_size sourceIndex = cubic ? ((i * 3) + 1) : i;
                    float values[4] {0.0f, 0.0f, 0.0f, 1.0f};
                    cgltf_accessor_read_float(sampler.output, sourceIndex, values, components);
                    out.values[i] = {values[0], values[1], values[2], values[3]};
                }
            }
        }

        for (cgltf_size channelIndex = 0; channelIndex < animation.channels_count; ++channelIndex) {
            const cgltf_animation_channel& channel = animation.channels[channelIndex];
            GltfAnimationChannelData& out = clip.channels[channelIndex];
            out.targetNode = nodeIndex(data, channel.target_node);
            out.path = animationPath(channel.target_path);
            for (cgltf_size samplerIndex = 0; samplerIndex < animation.samplers_count; ++samplerIndex) {
                if (&animation.samplers[samplerIndex] == channel.sampler) {
                    out.samplerIndex = static_cast<std::int32_t>(samplerIndex);
                    break;
                }
            }
        }

        result.animations.push_back(std::move(clip));
    }
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
    readNodes(data, result);
    readSkins(data, result);
    readAnimations(data, result);

    const std::vector<std::int32_t> meshSkins = meshSkinBindings(data);

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
            const cgltf_accessor* jointsAcc = findAttribute(prim, cgltf_attribute_type_joints, 0);
            const cgltf_accessor* weightsAcc = findAttribute(prim, cgltf_attribute_type_weights, 0);

            GltfPrimitiveData primData;
            if (meshIndex < meshSkins.size()) {
                primData.skinIndex = meshSkins[static_cast<std::size_t>(meshIndex)];
            }
            primData.hasSkinning = primData.skinIndex >= 0 && jointsAcc != nullptr && weightsAcc != nullptr;
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

                if (primData.hasSkinning) {
                    cgltf_uint joints[4] {0, 0, 0, 0};
                    float weights[4] {0.0f, 0.0f, 0.0f, 0.0f};
                    cgltf_accessor_read_uint(jointsAcc, vi, joints, 4);
                    cgltf_accessor_read_float(weightsAcc, vi, weights, 4);

                    float weightSum = 0.0f;
                    for (int influence = 0; influence < 4; ++influence) {
                        primData.vertices[vi].joints[static_cast<std::size_t>(influence)] =
                            static_cast<std::uint16_t>(std::min<cgltf_uint>(joints[influence], 95));
                        primData.vertices[vi].weights[static_cast<std::size_t>(influence)] = weights[influence];
                        weightSum += weights[influence];
                    }
                    if (weightSum > 0.00001f) {
                        for (float& weight : primData.vertices[vi].weights) {
                            weight /= weightSum;
                        }
                    } else {
                        primData.vertices[vi].joints = {0, 0, 0, 0};
                        primData.vertices[vi].weights = {1.0f, 0.0f, 0.0f, 0.0f};
                    }
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
