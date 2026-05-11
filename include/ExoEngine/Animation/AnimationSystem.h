#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <ExoEngine/Assets/GltfLoader.h>
#include <ExoEngine/Math/Mat4.h>
#include <ExoEngine/Scene/Scene.h>

namespace Exo {

class AnimationSystem {
public:
    void clear();
    void update(float deltaSeconds);

    void setTransform(const std::string& entityId, Transform transform);
    void animateTransform(
        const std::string& entityId,
        Transform start,
        Transform target,
        float durationSeconds,
        std::string easing = "linear");
    void clearTransform(const std::string& entityId);

    [[nodiscard]] bool hasTransform(const std::string& entityId) const;
    [[nodiscard]] Transform transformFor(const std::string& entityId, Transform fallback) const;

    bool registerRig(const std::string& entityId, GltfModelData model, const std::string& defaultClip = {});
    void playClip(const std::string& entityId, const std::string& clipId, bool loop, float fadeSeconds = 0.0f);
    void stopClip(const std::string& entityId, float fadeSeconds = 0.0f);
    void setPlaybackSpeed(const std::string& entityId, float speed);
    void setFacialCue(const std::string& entityId, const std::string& cueId, float intensity, float durationSeconds);
    [[nodiscard]] bool hasRig(const std::string& entityId) const;
    [[nodiscard]] bool hasClip(const std::string& entityId, const std::string& clipId) const;
    [[nodiscard]] const std::vector<Mat4>* jointMatricesFor(const std::string& entityId) const;

private:
    struct TransformAnimation {
        Transform start {};
        Transform target {};
        std::string easing = "linear";
        float duration = 0.0f;
        float elapsed = 0.0f;
    };

    struct NodePose {
        Vec3 translation {};
        Quat rotation {};
        Vec3 scale {1.0f, 1.0f, 1.0f};
        Mat4 matrix = Mat4::identity();
        bool hasMatrix = false;
    };

    struct FacialCueState {
        std::string cueId;
        float duration = 0.0f;
        float elapsed = 0.0f;
        float intensity = 1.0f;
        bool warnedMissingControls = false;
    };

    struct RigRuntime {
        GltfModelData model;
        std::string activeClip;
        std::string previousClip;
        bool loop = true;
        float playbackSpeed = 1.0f;
        float clipTime = 0.0f;
        float blendDuration = 0.0f;
        float blendElapsed = 0.0f;
        std::vector<NodePose> currentPose;
        std::vector<NodePose> blendFromPose;
        std::vector<Mat4> finalJointMatrices;
        FacialCueState facialCue;
        std::int32_t jawNode = -1;
        std::int32_t headNode = -1;
        std::int32_t neckNode = -1;
    };

    void updateRig(RigRuntime& rig, float deltaSeconds);
    [[nodiscard]] std::vector<NodePose> defaultPose(const GltfModelData& model) const;
    [[nodiscard]] std::vector<NodePose> sampledPose(const RigRuntime& rig, const std::string& clipId, float timeSeconds) const;
    void applyFacialCue(RigRuntime& rig, std::vector<NodePose>& pose, float deltaSeconds);
    void buildJointMatrices(RigRuntime& rig, const std::vector<NodePose>& pose);

    std::unordered_map<std::string, Transform> transformOverrides_;
    std::unordered_map<std::string, TransformAnimation> transformAnimations_;
    std::unordered_map<std::string, RigRuntime> rigs_;
};

} // namespace Exo
