#pragma once

#include <string>

#include <ExoEngine/Game/GameState.h>

namespace Exo {

class InventorySystem {
public:
    [[nodiscard]] static bool hasItem(const GameState& state, const std::string& itemId);
    static void addItem(GameState& state, const std::string& itemId);
    static bool removeItem(GameState& state, const std::string& itemId);
};

} // namespace Exo
