#include <ExoEngine/Game/RoomManager.h>

#include <ExoEngine/Core/Logger.h>

#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace Exo {
namespace {

using Json = nlohmann::json;

std::string childPath(std::string_view parent, std::string_view key) {
    std::string path(parent);
    path += ".";
    path += key;
    return path;
}

std::string childPath(std::string_view parent, std::size_t index) {
    std::string path(parent);
    path += "[";
    path += std::to_string(index);
    path += "]";
    return path;
}

class RoomJsonReader {
public:
    RoomJsonReader(const Json& root, std::filesystem::path sourcePath)
        : root_(root), sourcePath_(std::move(sourcePath)) {}

    [[nodiscard]] RoomDefinition read() const {
        expectObject(root_, "$");

        RoomDefinition room;
        room.sourcePath = sourcePath_;
        room.id = requiredString(root_, "id", "$");
        room.title = optionalString(root_, "title", "$", room.id);
        room.scenePath = optionalPath(root_, "scene", "$");
        room.collapseAfterFlag = optionalString(root_, "collapseAfterFlag", "$", "");
        room.collapseSequenceId = optionalString(root_, "collapseSequenceId", "$", "");
        room.collapseAudioCue = optionalString(root_, "collapseAudioCue", "$", "");
        room.collapseHeartbeatAudioCue = optionalString(root_, "collapseHeartbeatAudioCue", "$", "");
        room.collapseTargetRoom = optionalString(root_, "collapseTargetRoom", "$", "");
        room.collapseTargetSpawn = optionalString(root_, "collapseTargetSpawn", "$", room.collapseTargetSpawn);
        room.collapseTargetEnteredFlag = optionalString(root_, "collapseTargetEnteredFlag", "$", "");
        room.walkBounds = optionalBounds(root_, "walkBounds", "$", room.walkBounds);
        room.spawns = readSpawns();
        room.interactions = readInteractions();
        room.doors = readDoors();
        room.triggers = readTriggers();
        room.roomEnterEvents = readRoomEnterEvents();
        room.audioCues = readAudioCues();
        room.musicPath = readMusicPath();
        room.collisionBoxes = readCollisionBoxes();
        room.sequences = readSequences();

        if (room.spawns.empty()) {
            room.spawns.push_back({});
        }

        return room;
    }

private:
    [[noreturn]] void fail(std::string_view path, std::string_view message) const {
        throw std::runtime_error(
            "Room JSON error in " + sourcePath_.string() + " at " + std::string(path) + ": " + std::string(message));
    }

    void expectObject(const Json& value, std::string_view path) const {
        if (!value.is_object()) {
            fail(path, "expected object");
        }
    }

    void expectArray(const Json& value, std::string_view path) const {
        if (!value.is_array()) {
            fail(path, "expected array");
        }
    }

    [[nodiscard]] const Json* optionalField(const Json& object, std::string_view field) const {
        if (!object.is_object()) {
            return nullptr;
        }
        const auto it = object.find(std::string(field));
        return it == object.end() ? nullptr : &(*it);
    }

    [[nodiscard]] const Json& requiredField(const Json& object, std::string_view field, std::string_view path) const {
        const Json* value = optionalField(object, field);
        if (value == nullptr) {
            fail(childPath(path, field), "missing required field");
        }
        return *value;
    }

    [[nodiscard]] std::string requiredString(const Json& object, std::string_view field, std::string_view path) const {
        const Json& value = requiredField(object, field, path);
        if (!value.is_string()) {
            fail(childPath(path, field), "expected string");
        }
        const std::string result = value.get<std::string>();
        if (result.empty()) {
            fail(childPath(path, field), "must not be empty");
        }
        return result;
    }

    [[nodiscard]] std::string optionalString(
        const Json& object,
        std::string_view field,
        std::string_view path,
        std::string fallback) const {
        const Json* value = optionalField(object, field);
        if (value == nullptr) {
            return fallback;
        }
        if (!value->is_string()) {
            fail(childPath(path, field), "expected string");
        }
        return value->get<std::string>();
    }

    [[nodiscard]] bool optionalBool(
        const Json& object,
        std::string_view field,
        std::string_view path,
        bool fallback) const {
        const Json* value = optionalField(object, field);
        if (value == nullptr) {
            return fallback;
        }
        if (!value->is_boolean()) {
            fail(childPath(path, field), "expected boolean");
        }
        return value->get<bool>();
    }

    [[nodiscard]] int optionalInt(
        const Json& object,
        std::string_view field,
        std::string_view path,
        int fallback) const {
        const Json* value = optionalField(object, field);
        if (value == nullptr) {
            return fallback;
        }
        if (!value->is_number_integer()) {
            fail(childPath(path, field), "expected integer");
        }
        return value->get<int>();
    }

    [[nodiscard]] float number(const Json& value, std::string_view path) const {
        if (!value.is_number()) {
            fail(path, "expected number");
        }
        const double raw = value.get<double>();
        if (!std::isfinite(raw)) {
            fail(path, "must be finite");
        }
        return static_cast<float>(raw);
    }

    [[nodiscard]] float optionalNumber(
        const Json& object,
        std::string_view field,
        std::string_view path,
        float fallback) const {
        const Json* value = optionalField(object, field);
        if (value == nullptr) {
            return fallback;
        }
        return number(*value, childPath(path, field));
    }

    [[nodiscard]] Vec3 vec3(const Json& value, std::string_view path) const {
        expectArray(value, path);
        if (value.size() != 3) {
            fail(path, "expected exactly 3 numbers");
        }
        return {
            number(value[0], childPath(path, 0)),
            number(value[1], childPath(path, 1)),
            number(value[2], childPath(path, 2)),
        };
    }

    [[nodiscard]] Vec3 requiredVec3(const Json& object, std::string_view field, std::string_view path) const {
        return vec3(requiredField(object, field, path), childPath(path, field));
    }

    [[nodiscard]] bool optionalVec3(
        const Json& object,
        std::string_view field,
        std::string_view path,
        Vec3& out) const {
        const Json* value = optionalField(object, field);
        if (value == nullptr) {
            return false;
        }
        out = vec3(*value, childPath(path, field));
        return true;
    }

    [[nodiscard]] Bounds3 bounds(const Json& value, std::string_view path) const {
        expectObject(value, path);
        Bounds3 result;
        result.min = requiredVec3(value, "min", path);
        result.max = requiredVec3(value, "max", path);
        if (result.min.x > result.max.x || result.min.y > result.max.y || result.min.z > result.max.z) {
            fail(path, "min must be less than or equal to max on every axis");
        }
        return result;
    }

    [[nodiscard]] Bounds3 optionalBounds(
        const Json& object,
        std::string_view field,
        std::string_view path,
        Bounds3 fallback) const {
        const Json* value = optionalField(object, field);
        return value == nullptr ? fallback : bounds(*value, childPath(path, field));
    }

    [[nodiscard]] std::filesystem::path optionalPath(
        const Json& object,
        std::string_view field,
        std::string_view path) const {
        const std::string value = optionalString(object, field, path, "");
        return value.empty() ? std::filesystem::path() : std::filesystem::path(value);
    }

    [[nodiscard]] std::vector<RoomSpawn> readSpawns() const {
        const Json* spawnsJson = optionalField(root_, "spawns");
        const bool legacySpawnPoints = spawnsJson == nullptr;
        if (spawnsJson == nullptr) {
            spawnsJson = optionalField(root_, "spawnPoints");
        }
        if (spawnsJson == nullptr) {
            return {};
        }
        expectArray(*spawnsJson, legacySpawnPoints ? "$.spawnPoints" : "$.spawns");

        std::vector<RoomSpawn> spawns;
        for (std::size_t i = 0; i < spawnsJson->size(); ++i) {
            const Json& spawnJson = (*spawnsJson)[i];
            const std::string path = childPath(legacySpawnPoints ? "$.spawnPoints" : "$.spawns", i);
            expectObject(spawnJson, path);

            RoomSpawn spawn;
            spawn.id = requiredString(spawnJson, "id", path);
            spawn.position = requiredVec3(spawnJson, "position", path);
            if (spawn.position.y <= 0.01f) {
                spawn.position.y = 1.65f;
            }
            spawn.yaw = optionalNumber(spawnJson, "yaw", path, spawn.yaw);
            if (const Json* yawDeg = optionalField(spawnJson, "yawDeg")) {
                spawn.yaw = number(*yawDeg, childPath(path, "yawDeg")) * 0.017453292519943295769f;
            }
            spawns.push_back(std::move(spawn));
        }
        return spawns;
    }

    [[nodiscard]] std::vector<RoomInteraction> readInteractions() const {
        const Json* interactionsJson = optionalField(root_, "interactions");
        const bool legacyInteractables = interactionsJson == nullptr;
        if (interactionsJson == nullptr) {
            interactionsJson = optionalField(root_, "interactables");
        }
        if (interactionsJson == nullptr) {
            return {};
        }
        expectArray(*interactionsJson, legacyInteractables ? "$.interactables" : "$.interactions");

        std::vector<RoomInteraction> interactions;
        for (std::size_t i = 0; i < interactionsJson->size(); ++i) {
            const Json& interactionJson = (*interactionsJson)[i];
            const std::string path = childPath(legacyInteractables ? "$.interactables" : "$.interactions", i);
            expectObject(interactionJson, path);

            RoomInteraction interaction;
            interaction.id = requiredString(interactionJson, "id", path);
            interaction.type = optionalString(interactionJson, "type", path, "inspect");
            interaction.prompt = optionalString(
                interactionJson,
                legacyInteractables ? "choiceText" : "prompt",
                path,
                interaction.id);
            interaction.bounds = optionalBounds(interactionJson, "bounds", path, interaction.bounds);
            if (legacyInteractables && optionalField(interactionJson, "position") != nullptr) {
                const Vec3 position = requiredVec3(interactionJson, "position", path);
                const float radius = optionalNumber(interactionJson, "radius", path, 0.55f);
                interaction.bounds = {
                    {position.x - radius, -0.5f, position.z - radius},
                    {position.x + radius, 2.5f, position.z + radius},
                };
            }
            interaction.storyNode = optionalString(interactionJson, "storyNode", path, "");
            interaction.uiOverlay = optionalString(interactionJson, "uiOverlay", path, "");
            interaction.setFlag = optionalString(interactionJson, "setFlag", path, "");
            if (interaction.setFlag.empty()) {
                if (const Json* flags = optionalField(interactionJson, "setsFlags");
                    flags != nullptr && flags->is_array() && !flags->empty() && (*flags)[0].is_string()) {
                    interaction.setFlag = (*flags)[0].get<std::string>();
                }
            }
            interaction.addItem = optionalString(interactionJson, "addItem", path, "");
            if (interaction.addItem.empty()) {
                if (const Json* items = optionalField(interactionJson, "itemRefs");
                    items != nullptr && items->is_array() && !items->empty() && (*items)[0].is_string()) {
                    interaction.addItem = (*items)[0].get<std::string>();
                }
            }
            interaction.requiredItem = optionalString(interactionJson, "requiredItem", path, "");
            if (interaction.requiredItem.empty()) {
                interaction.requiredItem = optionalString(interactionJson, "requiredItemId", path, "");
            }
            interaction.identityDelta = optionalInt(interactionJson, "identityDelta", path, 0);
            interaction.once = optionalBool(interactionJson, "once", path, true);
            interactions.push_back(std::move(interaction));
        }
        return interactions;
    }

    [[nodiscard]] std::vector<RoomDoor> readDoors() const {
        const Json* doorsJson = optionalField(root_, "doors");
        if (doorsJson == nullptr) {
            return {};
        }
        expectArray(*doorsJson, "$.doors");

        std::vector<RoomDoor> doors;
        for (std::size_t i = 0; i < doorsJson->size(); ++i) {
            const Json& doorJson = (*doorsJson)[i];
            const std::string path = childPath("$.doors", i);
            expectObject(doorJson, path);

            RoomDoor door;
            door.id = requiredString(doorJson, "id", path);
            door.prompt = optionalString(doorJson, "prompt", path, door.id);
            door.prompt = optionalString(doorJson, "choiceText", path, door.prompt);
            door.bounds = optionalBounds(doorJson, "bounds", path, door.bounds);
            door.targetRoom = optionalString(doorJson, "targetRoom", path, "");
            if (door.targetRoom.empty()) {
                door.targetRoom = optionalString(doorJson, "targetRoomId", path, "");
            }
            if (door.targetRoom.empty()) {
                fail(childPath(path, "targetRoom"), "missing required field");
            }
            door.targetSpawn = optionalString(doorJson, "targetSpawn", path, "entry");
            door.targetSpawn = optionalString(doorJson, "targetSpawnId", path, door.targetSpawn);
            door.storyNode = optionalString(doorJson, "storyNode", path, "");
            door.requiredItem = optionalString(doorJson, "requiredItem", path, "");
            if (door.requiredItem.empty()) {
                door.requiredItem = optionalString(doorJson, "requiredItemId", path, "");
            }
            door.locked = optionalBool(doorJson, "locked", path, false);
            if (!door.requiredItem.empty()) {
                door.locked = true;
            }
            doors.push_back(std::move(door));
        }
        return doors;
    }

    [[nodiscard]] std::vector<RoomTrigger> readTriggers() const {
        const Json* triggersJson = optionalField(root_, "triggers");
        if (triggersJson == nullptr) {
            return {};
        }
        expectArray(*triggersJson, "$.triggers");

        std::vector<RoomTrigger> triggers;
        for (std::size_t i = 0; i < triggersJson->size(); ++i) {
            const Json& triggerJson = (*triggersJson)[i];
            const std::string path = childPath("$.triggers", i);
            expectObject(triggerJson, path);

            RoomTrigger trigger;
            trigger.id = requiredString(triggerJson, "id", path);
            trigger.bounds = bounds(requiredField(triggerJson, "bounds", path), childPath(path, "bounds"));
            trigger.storyNode = optionalString(triggerJson, "storyNode", path, "");
            trigger.setFlag = optionalString(triggerJson, "setFlag", path, "");
            trigger.audioCue = optionalString(triggerJson, "audioCue", path, "");
            trigger.sequenceId = optionalString(triggerJson, "sequenceId", path, "");
            if (trigger.sequenceId.empty()) {
                trigger.sequenceId = optionalString(triggerJson, "startSequence", path, "");
            }
            trigger.identityDelta = optionalInt(triggerJson, "identityDelta", path, 0);
            trigger.once = optionalBool(triggerJson, "once", path, true);
            triggers.push_back(std::move(trigger));
        }
        return triggers;
    }

    [[nodiscard]] std::vector<RoomEnterEvent> readRoomEnterEvents() const {
        const Json* eventsJson = optionalField(root_, "roomEnterEvents");
        if (eventsJson == nullptr) {
            return {};
        }
        expectArray(*eventsJson, "$.roomEnterEvents");

        std::vector<RoomEnterEvent> events;
        for (std::size_t i = 0; i < eventsJson->size(); ++i) {
            const Json& eventJson = (*eventsJson)[i];
            const std::string path = childPath("$.roomEnterEvents", i);
            expectObject(eventJson, path);

            RoomEnterEvent event;
            event.id = requiredString(eventJson, "id", path);
            event.setFlag = optionalString(eventJson, "setFlag", path, "");
            if (event.setFlag.empty()) {
                if (const Json* flags = optionalField(eventJson, "setsFlags");
                    flags != nullptr && flags->is_array() && !flags->empty() && (*flags)[0].is_string()) {
                    event.setFlag = (*flags)[0].get<std::string>();
                }
            }
            event.audioCue = optionalString(eventJson, "audioCue", path, "");
            event.once = optionalBool(eventJson, "once", path, true);
            events.push_back(std::move(event));
        }
        return events;
    }

    [[nodiscard]] std::unordered_map<std::string, std::filesystem::path> readAudioCues() const {
        const Json* audioJson = optionalField(root_, "audio");
        if (audioJson == nullptr) {
            return {};
        }
        expectObject(*audioJson, "$.audio");

        const Json* cuesJson = optionalField(*audioJson, "cues");
        if (cuesJson == nullptr) {
            return {};
        }
        expectArray(*cuesJson, "$.audio.cues");

        std::unordered_map<std::string, std::filesystem::path> cues;
        for (std::size_t i = 0; i < cuesJson->size(); ++i) {
            const Json& cueJson = (*cuesJson)[i];
            const std::string path = childPath("$.audio.cues", i);
            expectObject(cueJson, path);

            const std::string id = requiredString(cueJson, "id", path);
            const std::filesystem::path filePath = optionalPath(cueJson, "path", path);
            if (filePath.empty()) {
                fail(childPath(path, "path"), "missing required field");
            }
            cues[id] = filePath;
        }
        return cues;
    }

    [[nodiscard]] std::filesystem::path readMusicPath() const {
        const Json* audioJson = optionalField(root_, "audio");
        if (audioJson == nullptr) {
            return {};
        }
        expectObject(*audioJson, "$.audio");
        return optionalPath(*audioJson, "musicPath", "$.audio");
    }

    [[nodiscard]] std::vector<RoomCollisionBox> readCollisionBoxes() const {
        const Json* boxesJson = optionalField(root_, "collisionBoxes");
        const bool legacyColliders = boxesJson == nullptr;
        if (boxesJson == nullptr) {
            boxesJson = optionalField(root_, "colliders");
        }
        if (boxesJson == nullptr) {
            return {};
        }
        expectArray(*boxesJson, legacyColliders ? "$.colliders" : "$.collisionBoxes");

        std::vector<RoomCollisionBox> boxes;
        for (std::size_t i = 0; i < boxesJson->size(); ++i) {
            const Json& boxJson = (*boxesJson)[i];
            const std::string path = childPath(legacyColliders ? "$.colliders" : "$.collisionBoxes", i);
            expectObject(boxJson, path);

            RoomCollisionBox box;
            box.id = optionalString(boxJson, "id", path, "collision_box_" + std::to_string(i + 1));
            box.bounds = bounds(requiredField(boxJson, "bounds", path), childPath(path, "bounds"));
            boxes.push_back(std::move(box));
        }
        return boxes;
    }

    [[nodiscard]] std::vector<RoomSequence> readSequences() const {
        const Json* sequencesJson = optionalField(root_, "sequences");
        if (sequencesJson == nullptr) {
            return {};
        }
        expectArray(*sequencesJson, "$.sequences");

        std::vector<RoomSequence> sequences;
        for (std::size_t i = 0; i < sequencesJson->size(); ++i) {
            const Json& sequenceJson = (*sequencesJson)[i];
            const std::string path = childPath("$.sequences", i);
            expectObject(sequenceJson, path);

            RoomSequence sequence;
            sequence.id = requiredString(sequenceJson, "id", path);
            sequence.autoStart = optionalBool(sequenceJson, "autoStart", path, false);
            sequence.once = optionalBool(sequenceJson, "once", path, true);
            sequence.conditions = readSequenceConditions(sequenceJson, childPath(path, "conditions"));
            sequence.steps = readSequenceSteps(sequenceJson, childPath(path, "steps"));
            sequences.push_back(std::move(sequence));
        }
        return sequences;
    }

    [[nodiscard]] std::vector<SequenceCondition> readSequenceConditions(
        const Json& sequenceJson,
        std::string_view path) const {
        const Json* conditionsJson = optionalField(sequenceJson, "conditions");
        if (conditionsJson == nullptr) {
            return {};
        }
        expectArray(*conditionsJson, path);

        std::vector<SequenceCondition> conditions;
        for (std::size_t i = 0; i < conditionsJson->size(); ++i) {
            const Json& conditionJson = (*conditionsJson)[i];
            const std::string conditionPath = childPath(path, i);

            SequenceCondition condition;
            if (conditionJson.is_string()) {
                condition.flag = conditionJson.get<std::string>();
            } else {
                expectObject(conditionJson, conditionPath);
                condition.flag = optionalString(conditionJson, "flag", conditionPath, "");
                condition.negated = optionalBool(conditionJson, "not", conditionPath, false);
            }
            conditions.push_back(std::move(condition));
        }
        return conditions;
    }

    [[nodiscard]] std::vector<SequenceStep> readSequenceSteps(const Json& sequenceJson, std::string_view path) const {
        const Json* stepsJson = optionalField(sequenceJson, "steps");
        if (stepsJson == nullptr) {
            return {};
        }
        expectArray(*stepsJson, path);

        std::vector<SequenceStep> steps;
        for (std::size_t i = 0; i < stepsJson->size(); ++i) {
            const Json& stepJson = (*stepsJson)[i];
            const std::string stepPath = childPath(path, i);
            expectObject(stepJson, stepPath);

            SequenceStep step;
            step.time = optionalNumber(stepJson, "time", stepPath, 0.0f);
            step.actions = readSequenceActions(stepJson, childPath(stepPath, "actions"));
            steps.push_back(std::move(step));
        }
        return steps;
    }

    [[nodiscard]] std::vector<SequenceAction> readSequenceActions(const Json& stepJson, std::string_view path) const {
        const Json* actionsJson = optionalField(stepJson, "actions");
        if (actionsJson == nullptr) {
            return {};
        }
        expectArray(*actionsJson, path);

        std::vector<SequenceAction> actions;
        for (std::size_t i = 0; i < actionsJson->size(); ++i) {
            const Json& actionJson = (*actionsJson)[i];
            const std::string actionPath = childPath(path, i);
            expectObject(actionJson, actionPath);

            SequenceAction action;
            action.type = optionalString(actionJson, "type", actionPath, "");
            action.flag = optionalString(actionJson, "flag", actionPath, "");
            action.audioCue = optionalString(actionJson, "audioCue", actionPath, "");
            if (action.audioCue.empty()) {
                action.audioCue = optionalString(actionJson, "cue", actionPath, "");
            }
            action.cameraMode = optionalString(actionJson, "cameraMode", actionPath, "");
            if (action.cameraMode.empty()) {
                action.cameraMode = optionalString(actionJson, "mode", actionPath, "");
            }
            action.roomId = optionalString(actionJson, "roomId", actionPath, "");
            if (action.roomId.empty()) {
                action.roomId = optionalString(actionJson, "targetRoomId", actionPath, "");
            }
            if (action.roomId.empty()) {
                action.roomId = optionalString(actionJson, "targetRoom", actionPath, "");
            }
            action.spawnId = optionalString(actionJson, "spawnId", actionPath, "");
            if (action.spawnId.empty()) {
                action.spawnId = optionalString(actionJson, "targetSpawnId", actionPath, "");
            }
            if (action.spawnId.empty()) {
                action.spawnId = optionalString(actionJson, "targetSpawn", actionPath, "");
            }
            action.entityId = optionalString(actionJson, "entityId", actionPath, "");
            if (action.entityId.empty()) {
                action.entityId = optionalString(actionJson, "entity", actionPath, "");
            }
            action.cueId = optionalString(actionJson, "cueId", actionPath, "");
            if (action.cueId.empty()) {
                action.cueId = optionalString(actionJson, "performanceCue", actionPath, "");
            }
            action.clipId = optionalString(actionJson, "clipId", actionPath, "");
            if (action.clipId.empty()) {
                action.clipId = optionalString(actionJson, "animation", actionPath, "");
            }
            if (action.clipId.empty()) {
                action.clipId = optionalString(actionJson, "clip", actionPath, "");
            }
            action.easing = optionalString(actionJson, "easing", actionPath, "");
            action.hasPosition = optionalVec3(actionJson, "position", actionPath, action.position);
            action.hasRotation = optionalVec3(actionJson, "rotation", actionPath, action.rotation);
            action.hasScale = optionalVec3(actionJson, "scale", actionPath, action.scale);
            if (const Json* transformJson = optionalField(actionJson, "transform")) {
                expectObject(*transformJson, childPath(actionPath, "transform"));
                action.hasPosition = optionalVec3(*transformJson, "position", childPath(actionPath, "transform"), action.position)
                    || action.hasPosition;
                action.hasRotation = optionalVec3(*transformJson, "rotation", childPath(actionPath, "transform"), action.rotation)
                    || action.hasRotation;
                action.hasScale = optionalVec3(*transformJson, "scale", childPath(actionPath, "transform"), action.scale)
                    || action.hasScale;
            }
            if (const Json* targetTransformJson = optionalField(actionJson, "targetTransform")) {
                expectObject(*targetTransformJson, childPath(actionPath, "targetTransform"));
                action.hasPosition = optionalVec3(*targetTransformJson, "position", childPath(actionPath, "targetTransform"), action.position)
                    || action.hasPosition;
                action.hasRotation = optionalVec3(*targetTransformJson, "rotation", childPath(actionPath, "targetTransform"), action.rotation)
                    || action.hasRotation;
                action.hasScale = optionalVec3(*targetTransformJson, "scale", childPath(actionPath, "targetTransform"), action.scale)
                    || action.hasScale;
            }
            action.visible = optionalBool(actionJson, "visible", actionPath, action.visible);
            action.loop = optionalBool(actionJson, "loop", actionPath, action.loop);
            action.blackFade = optionalNumber(actionJson, "blackFade", actionPath, action.blackFade);
            action.blackFade = optionalNumber(actionJson, "value", actionPath, action.blackFade);
            action.noiseIntensity = optionalNumber(actionJson, "noiseIntensity", actionPath, action.noiseIntensity);
            action.duration = optionalNumber(actionJson, "duration", actionPath, action.duration);
            action.volume = optionalNumber(actionJson, "volume", actionPath, action.volume);
            action.intensity = optionalNumber(actionJson, "intensity", actionPath, action.intensity);
            action.fadeSeconds = optionalNumber(actionJson, "fadeSeconds", actionPath, action.fadeSeconds);
            action.playbackSpeed = optionalNumber(actionJson, "playbackSpeed", actionPath, action.playbackSpeed);
            action.playbackSpeed = optionalNumber(actionJson, "speed", actionPath, action.playbackSpeed);
            actions.push_back(std::move(action));
        }
        return actions;
    }

    const Json& root_;
    std::filesystem::path sourcePath_;
};

std::filesystem::path roomPath(const std::filesystem::path& dataRoot, const std::string& roomId) {
    const std::filesystem::path direct = dataRoot / "rooms" / roomId / "room.json";
    if (std::filesystem::exists(direct)) {
        return direct;
    }

    const std::filesystem::path roomsRoot = dataRoot / "rooms";
    if (!std::filesystem::exists(roomsRoot)) {
        return direct;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(roomsRoot)) {
        if (!entry.is_directory()) {
            continue;
        }

        const std::filesystem::path candidate = entry.path() / "room.json";
        std::ifstream input(candidate, std::ios::in | std::ios::binary);
        if (!input) {
            continue;
        }

        try {
            std::ostringstream buffer;
            buffer << input.rdbuf();
            const Json root = Json::parse(buffer.str());
            if (root.contains("id") && root["id"].is_string() && root["id"].get<std::string>() == roomId) {
                return candidate;
            }
        } catch (...) {
        }
    }

    return direct;
}

} // namespace

bool RoomManager::loadRoom(const std::filesystem::path& dataRoot, const std::string& roomId, const std::string& spawnId) {
    loaded_ = false;
    lastError_.clear();

    const std::filesystem::path path = roomPath(dataRoot, roomId);
    Logger::info("[RoomLoad] loading " + path.string());
    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input) {
        lastError_ = "Unable to open room JSON file: " + path.string();
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    try {
        const Json root = Json::parse(buffer.str());
        RoomJsonReader reader(root, path);
        currentRoom_ = reader.read();
    } catch (const std::exception& error) {
        lastError_ = error.what();
        return false;
    }

    activeSpawn_ = currentRoom_.spawns.front();
    bool matchedSpawn = spawnId.empty();
    for (const RoomSpawn& spawn : currentRoom_.spawns) {
        if (spawn.id == spawnId) {
            activeSpawn_ = spawn;
            matchedSpawn = true;
            break;
        }
    }
    if (!matchedSpawn) {
        Logger::warn("[RoomLoad] spawn not found: " + currentRoom_.id + "." + spawnId
            + "; fallback=" + activeSpawn_.id);
    }

    loaded_ = true;
    return true;
}

bool RoomManager::loaded() const {
    return loaded_;
}

const std::string& RoomManager::lastError() const {
    return lastError_;
}

const RoomDefinition& RoomManager::currentRoom() const {
    return currentRoom_;
}

const RoomSpawn& RoomManager::activeSpawn() const {
    return activeSpawn_;
}

const RoomInteraction* RoomManager::interactionAt(Vec3 position) const {
    if (!loaded_) {
        return nullptr;
    }
    for (const RoomInteraction& interaction : currentRoom_.interactions) {
        if (interaction.bounds.contains(position)) {
            return &interaction;
        }
    }
    return nullptr;
}

const RoomDoor* RoomManager::doorAt(Vec3 position) const {
    if (!loaded_) {
        return nullptr;
    }
    for (const RoomDoor& door : currentRoom_.doors) {
        if (door.bounds.contains(position)) {
            return &door;
        }
    }
    return nullptr;
}

const RoomTrigger* RoomManager::triggerAt(Vec3 position) const {
    if (!loaded_) {
        return nullptr;
    }
    for (const RoomTrigger& trigger : currentRoom_.triggers) {
        if (trigger.bounds.contains(position)) {
            return &trigger;
        }
    }
    return nullptr;
}

} // namespace Exo
