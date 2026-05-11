#include <ExoEngine/Game/SequenceManager.h>

#include <algorithm>
#include <utility>

namespace Exo {

void SequenceManager::setSequences(std::vector<RoomSequence> sequences, const std::unordered_set<std::string>& flags) {
    sequences_.clear();
    active_.clear();
    completedOnce_.clear();

    for (RoomSequence& sequence : sequences) {
        if (sequence.id.empty()) {
            continue;
        }
        const std::string id = sequence.id;
        sequences_.emplace(id, std::move(sequence));
    }

    startAutoSequences(flags);
}

bool SequenceManager::startSequence(const std::string& id, const std::unordered_set<std::string>& flags) {
    const auto found = sequences_.find(id);
    if (found == sequences_.end()) {
        return false;
    }

    const RoomSequence& sequence = found->second;
    if (sequence.once && completedOnce_.contains(id)) {
        return false;
    }
    if (isActive(id) || !conditionsMet(sequence, flags)) {
        return false;
    }

    ActiveSequence active;
    active.id = id;
    active.executedSteps.assign(sequence.steps.size(), false);
    active_.push_back(std::move(active));
    return true;
}

void SequenceManager::startAutoSequences(const std::unordered_set<std::string>& flags) {
    for (const auto& [id, sequence] : sequences_) {
        if (sequence.autoStart) {
            (void)startSequence(id, flags);
        }
    }
}

void SequenceManager::stopSequence(const std::string& id) {
    active_.erase(
        std::remove_if(active_.begin(), active_.end(), [&](const ActiveSequence& sequence) {
            return sequence.id == id;
        }),
        active_.end());
}

std::vector<SequenceAction> SequenceManager::update(float deltaSeconds) {
    std::vector<SequenceAction> actions;
    const float dt = std::max(deltaSeconds, 0.0f);

    for (ActiveSequence& active : active_) {
        const auto found = sequences_.find(active.id);
        if (found == sequences_.end()) {
            continue;
        }

        const RoomSequence& sequence = found->second;
        active.elapsed += dt;

        for (std::size_t i = 0; i < sequence.steps.size(); ++i) {
            if (active.executedSteps[i] || sequence.steps[i].time > active.elapsed) {
                continue;
            }
            active.executedSteps[i] = true;
            actions.insert(actions.end(), sequence.steps[i].actions.begin(), sequence.steps[i].actions.end());
        }
    }

    active_.erase(
        std::remove_if(active_.begin(), active_.end(), [&](const ActiveSequence& active) {
            const auto found = sequences_.find(active.id);
            if (found == sequences_.end()) {
                return true;
            }
            const bool finished = std::ranges::all_of(active.executedSteps, [](bool executed) {
                return executed;
            });
            if (finished && found->second.once) {
                completedOnce_.insert(active.id);
            }
            return finished;
        }),
        active_.end());

    return actions;
}

bool SequenceManager::running() const {
    return !active_.empty();
}

bool SequenceManager::conditionsMet(const RoomSequence& sequence, const std::unordered_set<std::string>& flags) const {
    for (const SequenceCondition& condition : sequence.conditions) {
        if (condition.flag.empty()) {
            continue;
        }
        const bool hasFlag = flags.contains(condition.flag);
        if ((!condition.negated && !hasFlag) || (condition.negated && hasFlag)) {
            return false;
        }
    }
    return true;
}

bool SequenceManager::isActive(const std::string& id) const {
    return std::ranges::any_of(active_, [&](const ActiveSequence& sequence) {
        return sequence.id == id;
    });
}

} // namespace Exo
