#include <ExoEngine/Game/PuzzleSystem.h>

namespace Exo {

bool PuzzleSystem::isGateOpen(const GameState& state, const PuzzleGate& gate) {
    const bool flagOk = gate.requiredFlag.empty() || state.flags.contains(gate.requiredFlag);
    const bool itemOk = gate.requiredItem.empty() || state.inventory.contains(gate.requiredItem);
    return flagOk && itemOk;
}

} // namespace Exo
