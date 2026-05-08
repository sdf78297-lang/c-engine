#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <ExoEngine/Scene/FixedCameraRig.h>

namespace Exo {

struct RoomSpawn {
    std::string id = "entry";
    Vec3 position {0.0f, 1.65f, 1.5f};
    float yaw = 3.1415926f;
};

struct RoomInteraction {
    std::string id;
    std::string type;
    std::string prompt;
    Bounds3 bounds {};
    std::string storyNode;
    std::string setFlag;
    std::string addItem;
    std::string requiredItem;
    int identityDelta = 0;
    bool once = true;
};

struct RoomDoor {
    std::string id;
    std::string prompt;
    Bounds3 bounds {};
    std::string targetRoom;
    std::string targetSpawn = "entry";
    std::string storyNode;
    std::string requiredItem;
    bool locked = false;
};

struct RoomTrigger {
    std::string id;
    Bounds3 bounds {};
    std::string storyNode;
    std::string setFlag;
    int identityDelta = 0;
    bool once = true;
};

struct RoomCollisionBox {
    std::string id;
    Bounds3 bounds {};
};

struct RoomDefinition {
    std::string id = "office_open_space";
    std::string title = "Office";
    std::filesystem::path sourcePath;
    std::filesystem::path scenePath;
    Bounds3 walkBounds {{-1.85f, 0.0f, -1.85f}, {1.85f, 2.4f, 1.85f}};
    std::vector<RoomSpawn> spawns;
    std::vector<RoomInteraction> interactions;
    std::vector<RoomDoor> doors;
    std::vector<RoomTrigger> triggers;
    std::vector<RoomCollisionBox> collisionBoxes;
};

class RoomManager {
public:
    [[nodiscard]] bool loadRoom(const std::filesystem::path& dataRoot, const std::string& roomId, const std::string& spawnId = "entry");
    [[nodiscard]] bool loaded() const;
    [[nodiscard]] const std::string& lastError() const;
    [[nodiscard]] const RoomDefinition& currentRoom() const;
    [[nodiscard]] const RoomSpawn& activeSpawn() const;
    [[nodiscard]] const RoomInteraction* interactionAt(Vec3 position) const;
    [[nodiscard]] const RoomDoor* doorAt(Vec3 position) const;
    [[nodiscard]] const RoomTrigger* triggerAt(Vec3 position) const;

private:
    std::string lastError_;
    RoomDefinition currentRoom_;
    RoomSpawn activeSpawn_;
    bool loaded_ = false;
};

} // namespace Exo
