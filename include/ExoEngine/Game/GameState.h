#pragma once

#include <string>
#include <unordered_set>

#include <ExoEngine/Math/Vec.h>

namespace Exo {

struct GameState {
    std::string roomId = "office_open_space_3f";
    std::string spawnId = "from_elevator_day";
    std::string storyNodeId;
    Vec3 playerPosition {0.0f, 1.65f, 1.5f};
    float playerYaw = 3.1415926f;
    int identity = 100;
    std::unordered_set<std::string> inventory;
    std::unordered_set<std::string> flags;
};

} // namespace Exo
