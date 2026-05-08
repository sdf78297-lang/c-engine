#pragma once

#include <string>

#include <ExoEngine/Game/GameState.h>
#include <ExoEngine/Game/RoomManager.h>

namespace Exo {

struct InteractionResult {
    bool activated = false;
    std::string storyNode;
    std::string message;
};

class InteractionSystem {
public:
    [[nodiscard]] static InteractionResult activate(GameState& state, const RoomInteraction& interaction);
    [[nodiscard]] static InteractionResult enterTrigger(GameState& state, const RoomTrigger& trigger);
};

} // namespace Exo
