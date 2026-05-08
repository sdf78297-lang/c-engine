#include <ExoEngine/Game/RoomManager.h>

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
        room.walkBounds = optionalBounds(root_, "walkBounds", "$", room.walkBounds);
        room.spawns = readSpawns();
        room.interactions = readInteractions();
        room.doors = readDoors();
        room.triggers = readTriggers();

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
            trigger.identityDelta = optionalInt(triggerJson, "identityDelta", path, 0);
            trigger.once = optionalBool(triggerJson, "once", path, true);
            triggers.push_back(std::move(trigger));
        }
        return triggers;
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
    for (const RoomSpawn& spawn : currentRoom_.spawns) {
        if (spawn.id == spawnId) {
            activeSpawn_ = spawn;
            break;
        }
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
