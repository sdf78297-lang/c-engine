#pragma once

#include <string>

#include <ExoEngine/Game/GameState.h>

namespace Exo {

struct PuzzleGate {
    std::string id;
    std::string requiredFlag;
    std::string requiredItem;
};

class PuzzleSystem {
public:
    [[nodiscard]] static bool isGateOpen(const GameState& state, const PuzzleGate& gate);
};

} // namespace Exo
