#include <ExoEngine/Game/InventorySystem.h>

namespace Exo {

bool InventorySystem::hasItem(const GameState& state, const std::string& itemId) {
    return itemId.empty() || state.inventory.contains(itemId);
}

void InventorySystem::addItem(GameState& state, const std::string& itemId) {
    if (!itemId.empty()) {
        state.inventory.insert(itemId);
    }
}

bool InventorySystem::removeItem(GameState& state, const std::string& itemId) {
    if (itemId.empty()) {
        return false;
    }
    return state.inventory.erase(itemId) > 0;
}

} // namespace Exo
