#include <ExoEngine/Animation/AnimationSystem.h>

#include <ExoEngine/Core/Logger.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <utility>

namespace Exo {
namespace {

float smoothStep01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float lerpFloat(float a, float b, float t) {
    return a + (b - a) * t;
}

Vec3 lerpVec3(Vec3 a, Vec3 b, float t) {
    return {
        lerpFloat(a.x, b.x, t),
        lerpFloat(a.y, b.y, t),
        lerpFloat(a.z, b.z, t),
    };
}

Transform lerpTransform(const Transform& a, const Transform& b, float t) {
    return {
        .position = lerpVec3(a.position, b.position, t),
        .rotation = lerpVec3(a.rotation, b.rotation, t),
        .scale = lerpVec3(a.scale, b.scale, t),
    };
}

Quat normalizeQuat(Quat q) {
    const float len = std::sqrt((q.x * q.x) + (q.y * q.y) + (q.z * q.z) + (q.w * q.w));
    if (len <= 0.00001f) {
        return {};
    }
    const float inv = 1.0f / len;
    return {q.x * inv, q.y * inv, q.z * inv, q.w * inv};
}

float dotQuat(Quat a, Quat b) {
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w);
}

Quat slerpQuat(Quat a, Quat b, float t) {
    a = normalizeQuat(a);
    b = normalizeQuat(b);
    float cosTheta = dotQuat(a, b);
    if (cosTheta < 0.0f) {
        b = {-b.x, -b.y, -b.z, -b.w};
        cosTheta = -cosTheta;
    }

    if (cosTheta > 0.9995f) {
        return normalizeQuat({
            lerpFloat(a.x, b.x, t),
            lerpFloat(a.y, b.y, t),
            lerpFloat(a.z, b.z, t),
            lerpFloat(a.w, b.w, t),
        });
    }

    const float theta = std::acos(std::clamp(cosTheta, -1.0f, 1.0f));
    const float sinTheta = std::sin(theta);
    const float wa = std::sin((1.0f - t) * theta) / sinTheta;
    const float wb = std::sin(t * theta) / sinTheta;
    return {
        (a.x * wa) + (b.x * wb),
        (a.y * wa) + (b.y * wb),
        (a.z * wa) + (b.z * wb),
        (a.w * wa) + (b.w * wb),
    };
}

Quat multiplyQuat(Quat a, Quat b) {
    return normalizeQuat({
        (a.w * b.x) + (a.x * b.w) + (a.y * b.z) - (a.z * b.y),
        (a.w * b.y) - (a.x * b.z) + (a.y * b.w) + (a.z * b.x),
        (a.w * b.z) + (a.x * b.y) - (a.y * b.x) + (a.z * b.w),
        (a.w * b.w) - (a.x * b.x) - (a.y * b.y) - (a.z * b.z),
    });
}

Quat axisAngle(Vec3 axis, float radians) {
    const Vec3 n = normalize(axis);
    const float half = radians * 0.5f;
    const float s = std::sin(half);
    return normalizeQuat({n.x * s, n.y * s, n.z * s, std::cos(half)});
}

Mat4 matrixFromQuat(Quat q) {
    q = normalizeQuat(q);
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float wx = q.w * q.x;
    const float wy = q.w * q.y;
    const float wz = q.w * q.z;

    Mat4 result = Mat4::identity();
    result.values[0] = 1.0f - (2.0f * (yy + zz));
    result.values[1] = 2.0f * (xy + wz);
    result.values[2] = 2.0f * (xz - wy);
    result.values[4] = 2.0f * (xy - wz);
    result.values[5] = 1.0f - (2.0f * (xx + zz));
    result.values[6] = 2.0f * (yz + wx);
    result.values[8] = 2.0f * (xz + wy);
    result.values[9] = 2.0f * (yz - wx);
    result.values[10] = 1.0f - (2.0f * (xx + yy));
    return result;
}

Mat4 composeMatrix(Vec3 translation, Quat rotation, Vec3 scale) {
    return Mat4::translate(translation) * matrixFromQuat(rotation) * Mat4::scale(scale);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool containsLower(const std::string& value, const std::string& needle) {
    return lower(value).find(needle) != std::string::npos;
}

const GltfAnimationClipData* findClip(const GltfModelData& model, const std::string& clipId) {
    for (const GltfAnimationClipData& clip : model.animations) {
        if (clip.name == clipId) {
            return &clip;
        }
    }
    return nullptr;
}

Vec4 sampleLinear(const GltfAnimationSamplerData& sampler, float time, bool rotation) {
    if (sampler.times.empty() || sampler.values.empty()) {
        return {};
    }
    if (time <= sampler.times.front()) {
        return sampler.values.front();
    }
    if (time >= sampler.times.back()) {
        return sampler.values.back();
    }

    std::size_t right = 1;
    while (right < sampler.times.size() && sampler.times[right] < time) {
        ++right;
    }
    const std::size_t left = right == 0 ? 0 : right - 1;
    const float span = std::max(sampler.times[right] - sampler.times[left], 0.00001f);
    const float t = std::clamp((time - sampler.times[left]) / span, 0.0f, 1.0f);
    if (sampler.interpolation == "STEP") {
        return sampler.values[left];
    }

    const Vec4 a = sampler.values[left];
    const Vec4 b = sampler.values[right];
    if (rotation) {
        const Quat q = slerpQuat({a.x, a.y, a.z, a.w}, {b.x, b.y, b.z, b.w}, t);
        return {q.x, q.y, q.z, q.w};
    }
    return {
        lerpFloat(a.x, b.x, t),
        lerpFloat(a.y, b.y, t),
        lerpFloat(a.z, b.z, t),
        lerpFloat(a.w, b.w, t),
    };
}

float applyEasing(float t, const std::string& easing) {
    constexpr float kPi = 3.14159265358979323846f;
    t = std::clamp(t, 0.0f, 1.0f);
    if (easing == "smooth" || easing == "smoothstep") {
        return smoothStep01(t);
    }
    if (easing == "easeOutCubic") {
        const float inv = 1.0f - t;
        return 1.0f - (inv * inv * inv);
    }
    if (easing == "easeOutSine") {
        return std::sin((t * kPi) * 0.5f);
    }
    if (easing == "easeInOutSine") {
        return -(std::cos(kPi * t) - 1.0f) * 0.5f;
    }
    return t;
}

} // namespace

void AnimationSystem::clear() {
    transformOverrides_.clear();
    transformAnimations_.clear();
    rigs_.clear();
}

void AnimationSystem::update(float deltaSeconds) {
    const float dt = std::max(deltaSeconds, 0.0f);
    for (auto it = transformAnimations_.begin(); it != transformAnimations_.end();) {
        TransformAnimation& animation = it->second;
        animation.elapsed = std::min(animation.elapsed + dt, animation.duration);
        const float rawT = animation.duration > 0.0f ? animation.elapsed / animation.duration : 1.0f;
        const float t = applyEasing(rawT, animation.easing);
        transformOverrides_[it->first] = lerpTransform(animation.start, animation.target, t);

        if (animation.elapsed >= animation.duration) {
            transformOverrides_[it->first] = animation.target;
            it = transformAnimations_.erase(it);
        } else {
            ++it;
        }
    }

    for (auto& [entityId, rig] : rigs_) {
        (void)entityId;
        updateRig(rig, dt);
    }
}

void AnimationSystem::setTransform(const std::string& entityId, Transform transform) {
    transformAnimations_.erase(entityId);
    transformOverrides_[entityId] = transform;
}

void AnimationSystem::animateTransform(
    const std::string& entityId,
    Transform start,
    Transform target,
    float durationSeconds,
    std::string easing) {
    const float duration = std::max(durationSeconds, 0.0f);
    if (duration <= 0.0f) {
        setTransform(entityId, target);
        return;
    }

    transformOverrides_[entityId] = start;
    transformAnimations_[entityId] = {
        .start = start,
        .target = target,
        .easing = std::move(easing),
        .duration = duration,
        .elapsed = 0.0f,
    };
}

void AnimationSystem::clearTransform(const std::string& entityId) {
    transformAnimations_.erase(entityId);
    transformOverrides_.erase(entityId);
}

bool AnimationSystem::hasTransform(const std::string& entityId) const {
    return transformOverrides_.contains(entityId);
}

Transform AnimationSystem::transformFor(const std::string& entityId, Transform fallback) const {
    const auto found = transformOverrides_.find(entityId);
    return found == transformOverrides_.end() ? fallback : found->second;
}

bool AnimationSystem::registerRig(const std::string& entityId, GltfModelData model, const std::string& defaultClip) {
    if (entityId.empty()) {
        return false;
    }
    if (model.skins.empty()) {
        Logger::warn("Animation rig skipped for " + entityId + ": GLB has no skin");
        return false;
    }

    const std::size_t jointCount = model.skins.front().joints.size();
    if (jointCount > 96) {
        Logger::warn("Animation rig " + entityId + " has " + std::to_string(jointCount)
            + " joints; renderer v1 will use the first 96");
    }

    RigRuntime rig;
    rig.model = std::move(model);
    rig.currentPose = defaultPose(rig.model);
    rig.finalJointMatrices.assign(std::min<std::size_t>(jointCount, 96), Mat4::identity());

    for (std::size_t i = 0; i < rig.model.nodes.size(); ++i) {
        const std::string& name = rig.model.nodes[i].name;
        if (rig.jawNode < 0 && containsLower(name, "jaw")) {
            rig.jawNode = static_cast<std::int32_t>(i);
        }
        if (rig.headNode < 0 && containsLower(name, "head")) {
            rig.headNode = static_cast<std::int32_t>(i);
        }
        if (rig.neckNode < 0 && containsLower(name, "neck")) {
            rig.neckNode = static_cast<std::int32_t>(i);
        }
    }

    rigs_[entityId] = std::move(rig);
    buildJointMatrices(rigs_[entityId], rigs_[entityId].currentPose);
    Logger::info("Animation rig registered: " + entityId
        + " joints=" + std::to_string(jointCount)
        + " clips=" + std::to_string(rigs_[entityId].model.animations.size()));

    if (!defaultClip.empty()) {
        playClip(entityId, defaultClip, true, 0.0f);
    } else if (!rigs_[entityId].model.animations.empty()) {
        playClip(entityId, rigs_[entityId].model.animations.front().name, true, 0.0f);
    }
    return true;
}

void AnimationSystem::playClip(const std::string& entityId, const std::string& clipId, bool loop, float fadeSeconds) {
    auto found = rigs_.find(entityId);
    if (found == rigs_.end()) {
        Logger::warn("playAnimation ignored for " + entityId + ": no registered animation rig");
        return;
    }
    RigRuntime& rig = found->second;
    if (findClip(rig.model, clipId) == nullptr) {
        Logger::warn("playAnimation ignored for " + entityId + ": missing clip " + clipId);
        return;
    }

    rig.blendFromPose = rig.currentPose.empty() ? defaultPose(rig.model) : rig.currentPose;
    rig.blendDuration = std::max(fadeSeconds, 0.0f);
    rig.blendElapsed = 0.0f;
    rig.activeClip = clipId;
    rig.loop = loop;
    rig.clipTime = 0.0f;
    Logger::info("Animation clip started: " + entityId
        + " clip=" + clipId
        + " loop=" + (loop ? std::string("true") : std::string("false")));
}

void AnimationSystem::stopClip(const std::string& entityId, float fadeSeconds) {
    auto found = rigs_.find(entityId);
    if (found == rigs_.end()) {
        return;
    }
    RigRuntime& rig = found->second;
    rig.blendFromPose = rig.currentPose;
    rig.blendDuration = std::max(fadeSeconds, 0.0f);
    rig.blendElapsed = 0.0f;
    rig.activeClip.clear();
}

void AnimationSystem::setPlaybackSpeed(const std::string& entityId, float speed) {
    auto found = rigs_.find(entityId);
    if (found == rigs_.end()) {
        Logger::warn("setPlaybackSpeed ignored for " + entityId + ": no registered animation rig");
        return;
    }
    found->second.playbackSpeed = std::max(speed, 0.0f);
}

void AnimationSystem::setFacialCue(
    const std::string& entityId,
    const std::string& cueId,
    float intensity,
    float durationSeconds) {
    auto found = rigs_.find(entityId);
    if (found == rigs_.end()) {
        Logger::warn("setFacialCue ignored for " + entityId + ": no registered animation rig");
        return;
    }
    RigRuntime& rig = found->second;
    rig.facialCue = {
        .cueId = cueId,
        .duration = std::max(durationSeconds, 0.0f),
        .elapsed = 0.0f,
        .intensity = std::max(intensity, 0.0f),
        .warnedMissingControls = false,
    };
    if (rig.jawNode < 0 && rig.headNode < 0 && rig.neckNode < 0) {
        Logger::warn("setFacialCue has no jaw/head/neck controls on rig: " + entityId);
        rig.facialCue.warnedMissingControls = true;
    }
}

bool AnimationSystem::hasRig(const std::string& entityId) const {
    return rigs_.contains(entityId);
}

bool AnimationSystem::hasClip(const std::string& entityId, const std::string& clipId) const {
    const auto found = rigs_.find(entityId);
    return found != rigs_.end() && findClip(found->second.model, clipId) != nullptr;
}

const std::vector<Mat4>* AnimationSystem::jointMatricesFor(const std::string& entityId) const {
    const auto found = rigs_.find(entityId);
    if (found == rigs_.end() || found->second.finalJointMatrices.empty()) {
        return nullptr;
    }
    return &found->second.finalJointMatrices;
}

std::vector<AnimationSystem::NodePose> AnimationSystem::defaultPose(const GltfModelData& model) const {
    std::vector<NodePose> pose;
    pose.reserve(model.nodes.size());
    for (const GltfNodeData& node : model.nodes) {
        pose.push_back({
            .translation = node.translation,
            .rotation = node.rotation,
            .scale = node.scale,
            .matrix = node.matrix,
            .hasMatrix = node.hasMatrix,
        });
    }
    return pose;
}

std::vector<AnimationSystem::NodePose> AnimationSystem::sampledPose(
    const RigRuntime& rig,
    const std::string& clipId,
    float timeSeconds) const {
    std::vector<NodePose> pose = defaultPose(rig.model);
    const GltfAnimationClipData* clip = findClip(rig.model, clipId);
    if (clip == nullptr) {
        return pose;
    }

    for (const GltfAnimationChannelData& channel : clip->channels) {
        if (channel.targetNode < 0
            || static_cast<std::size_t>(channel.targetNode) >= pose.size()
            || channel.samplerIndex < 0
            || static_cast<std::size_t>(channel.samplerIndex) >= clip->samplers.size()) {
            continue;
        }

        const GltfAnimationSamplerData& sampler = clip->samplers[static_cast<std::size_t>(channel.samplerIndex)];
        const Vec4 value = sampleLinear(sampler, timeSeconds, channel.path == GltfAnimationPath::Rotation);
        NodePose& target = pose[static_cast<std::size_t>(channel.targetNode)];
        target.hasMatrix = false;

        switch (channel.path) {
        case GltfAnimationPath::Translation:
            target.translation = {value.x, value.y, value.z};
            break;
        case GltfAnimationPath::Rotation:
            target.rotation = normalizeQuat({value.x, value.y, value.z, value.w});
            break;
        case GltfAnimationPath::Scale:
            target.scale = {value.x, value.y, value.z};
            break;
        default:
            break;
        }
    }
    return pose;
}

void AnimationSystem::applyFacialCue(RigRuntime& rig, std::vector<NodePose>& pose, float deltaSeconds) {
    if (rig.facialCue.duration <= 0.0f || rig.facialCue.elapsed >= rig.facialCue.duration) {
        return;
    }

    rig.facialCue.elapsed = std::min(rig.facialCue.elapsed + std::max(deltaSeconds, 0.0f), rig.facialCue.duration);
    const float in = smoothStep01(std::min(rig.facialCue.elapsed / 0.16f, 1.0f));
    const float out = smoothStep01(std::min((rig.facialCue.duration - rig.facialCue.elapsed) / 0.28f, 1.0f));
    const float envelope = in * out * std::clamp(rig.facialCue.intensity, 0.0f, 2.5f);
    const bool urgent = containsLower(rig.facialCue.cueId, "losing") || containsLower(rig.facialCue.cueId, "urgent");
    const float pulse = std::abs(std::sin(rig.facialCue.elapsed * (urgent ? 16.0f : 11.0f))) * envelope;

    auto rotateNode = [&](std::int32_t node, Quat delta) {
        if (node < 0 || static_cast<std::size_t>(node) >= pose.size()) {
            return;
        }
        NodePose& target = pose[static_cast<std::size_t>(node)];
        target.hasMatrix = false;
        target.rotation = multiplyQuat(target.rotation, delta);
    };

    rotateNode(rig.jawNode, axisAngle({1.0f, 0.0f, 0.0f}, -0.22f * pulse));
    rotateNode(rig.headNode, axisAngle({1.0f, 0.0f, 0.0f}, (urgent ? -0.045f : -0.030f) * envelope));
    rotateNode(rig.neckNode, axisAngle({0.0f, 0.0f, 1.0f}, std::sin(rig.facialCue.elapsed * 5.0f) * 0.018f * envelope));
}

void AnimationSystem::buildJointMatrices(RigRuntime& rig, const std::vector<NodePose>& pose) {
    if (rig.model.nodes.empty() || rig.model.skins.empty()) {
        rig.finalJointMatrices.clear();
        return;
    }

    std::vector<Mat4> global(pose.size(), Mat4::identity());
    std::vector<bool> visited(pose.size(), false);

    std::function<Mat4(std::size_t)> buildGlobal = [&](std::size_t nodeIndex) -> Mat4 {
        if (nodeIndex >= pose.size()) {
            return Mat4::identity();
        }
        if (visited[nodeIndex]) {
            return global[nodeIndex];
        }

        const NodePose& nodePose = pose[nodeIndex];
        const Mat4 local = nodePose.hasMatrix
            ? nodePose.matrix
            : composeMatrix(nodePose.translation, nodePose.rotation, nodePose.scale);
        const std::int32_t parent = rig.model.nodes[nodeIndex].parent;
        global[nodeIndex] = parent >= 0
            ? buildGlobal(static_cast<std::size_t>(parent)) * local
            : local;
        visited[nodeIndex] = true;
        return global[nodeIndex];
    };

    for (std::size_t i = 0; i < pose.size(); ++i) {
        buildGlobal(i);
    }

    const GltfSkinData& skin = rig.model.skins.front();
    const std::size_t jointCount = std::min<std::size_t>(skin.joints.size(), 96);
    rig.finalJointMatrices.assign(jointCount, Mat4::identity());
    for (std::size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
        const std::int32_t node = skin.joints[jointIndex];
        const Mat4 inverseBind = jointIndex < skin.inverseBindMatrices.size()
            ? skin.inverseBindMatrices[jointIndex]
            : Mat4::identity();
        if (node >= 0 && static_cast<std::size_t>(node) < global.size()) {
            rig.finalJointMatrices[jointIndex] = global[static_cast<std::size_t>(node)] * inverseBind;
        }
    }
}

void AnimationSystem::updateRig(RigRuntime& rig, float deltaSeconds) {
    const float dt = std::max(deltaSeconds, 0.0f);
    std::vector<NodePose> pose = rig.currentPose.empty() ? defaultPose(rig.model) : rig.currentPose;

    const GltfAnimationClipData* clip = rig.activeClip.empty() ? nullptr : findClip(rig.model, rig.activeClip);
    if (clip != nullptr && clip->duration > 0.0f) {
        rig.clipTime += dt * rig.playbackSpeed;
        if (rig.loop) {
            rig.clipTime = std::fmod(rig.clipTime, clip->duration);
        } else {
            rig.clipTime = std::min(rig.clipTime, clip->duration);
        }
        pose = sampledPose(rig, rig.activeClip, rig.clipTime);
    }

    if (rig.blendDuration > 0.0f && !rig.blendFromPose.empty() && rig.blendFromPose.size() == pose.size()) {
        rig.blendElapsed = std::min(rig.blendElapsed + dt, rig.blendDuration);
        const float t = smoothStep01(rig.blendElapsed / rig.blendDuration);
        for (std::size_t i = 0; i < pose.size(); ++i) {
            pose[i].translation = lerpVec3(rig.blendFromPose[i].translation, pose[i].translation, t);
            pose[i].rotation = slerpQuat(rig.blendFromPose[i].rotation, pose[i].rotation, t);
            pose[i].scale = lerpVec3(rig.blendFromPose[i].scale, pose[i].scale, t);
            pose[i].hasMatrix = false;
        }
        if (rig.blendElapsed >= rig.blendDuration) {
            rig.blendDuration = 0.0f;
            rig.blendFromPose.clear();
        }
    }

    applyFacialCue(rig, pose, dt);
    rig.currentPose = pose;
    buildJointMatrices(rig, rig.currentPose);
}

} // namespace Exo
