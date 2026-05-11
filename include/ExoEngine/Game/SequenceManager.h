#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <ExoEngine/Math/Vec.h>

namespace Exo {

struct SequenceCondition {
    std::string flag;
    bool negated = false;
};

struct SequenceAction {
    std::string type;
    std::string flag;
    std::string audioCue;
    std::string cameraMode;
    std::string roomId;
    std::string spawnId;
    std::string entityId;
    std::string cueId;
    std::string clipId;
    std::string easing;
    Vec3 position {};
    Vec3 rotation {};
    Vec3 scale {1.0f, 1.0f, 1.0f};
    bool hasPosition = false;
    bool hasRotation = false;
    bool hasScale = false;
    bool visible = true;
    bool loop = false;
    float blackFade = 0.0f;
    float noiseIntensity = 0.0f;
    float duration = 0.0f;
    float volume = -1.0f;
    float intensity = 1.0f;
    float fadeSeconds = 0.0f;
    float playbackSpeed = 1.0f;
};

struct SequenceStep {
    float time = 0.0f;
    std::vector<SequenceAction> actions;
};

struct RoomSequence {
    std::string id;
    bool autoStart = false;
    bool once = true;
    std::vector<SequenceCondition> conditions;
    std::vector<SequenceStep> steps;
};

class SequenceManager {
public:
    void setSequences(std::vector<RoomSequence> sequences, const std::unordered_set<std::string>& flags);
    [[nodiscard]] bool startSequence(const std::string& id, const std::unordered_set<std::string>& flags);
    void startAutoSequences(const std::unordered_set<std::string>& flags);
    void stopSequence(const std::string& id);
    [[nodiscard]] std::vector<SequenceAction> update(float deltaSeconds);
    [[nodiscard]] bool running() const;

private:
    struct ActiveSequence {
        std::string id;
        float elapsed = 0.0f;
        std::vector<bool> executedSteps;
    };

    [[nodiscard]] bool conditionsMet(const RoomSequence& sequence, const std::unordered_set<std::string>& flags) const;
    [[nodiscard]] bool isActive(const std::string& id) const;

    std::unordered_map<std::string, RoomSequence> sequences_;
    std::vector<ActiveSequence> active_;
    std::unordered_set<std::string> completedOnce_;
};

} // namespace Exo
