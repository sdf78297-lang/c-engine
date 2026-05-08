#include <ExoEngine/Game/InteractionSystem.h>

#include <ExoEngine/Game/InventorySystem.h>

#include <algorithm>

namespace Exo {

namespace {

void applyIdentityDelta(GameState& state, int delta) {
    state.identity = std::clamp(state.identity + delta, 0, 100);
}

} // namespace

InteractionResult InteractionSystem::activate(GameState& state, const RoomInteraction& interaction) {
    if (!InventorySystem::hasItem(state, interaction.requiredItem)) {
        return {
            .activated = false,
            .storyNode = {},
            .message = "Missing required item: " + interaction.requiredItem,
        };
    }

    if (interaction.once && !interaction.setFlag.empty() && state.flags.contains(interaction.setFlag)) {
        return {
            .activated = false,
            .storyNode = {},
            .message = "Interaction already consumed: " + interaction.id,
        };
    }

    applyIdentityDelta(state, interaction.identityDelta);
    InventorySystem::addItem(state, interaction.addItem);
    if (!interaction.setFlag.empty()) {
        state.flags.insert(interaction.setFlag);
    }
    if (!interaction.storyNode.empty()) {
        state.storyNodeId = interaction.storyNode;
    }

    return {
        .activated = true,
        .storyNode = interaction.storyNode,
        .message = interaction.prompt,
    };
}

InteractionResult InteractionSystem::enterTrigger(GameState& state, const RoomTrigger& trigger) {
    if (trigger.once && !trigger.setFlag.empty() && state.flags.contains(trigger.setFlag)) {
        return {
            .activated = false,
            .storyNode = {},
            .message = "Trigger already consumed: " + trigger.id,
        };
    }

    applyIdentityDelta(state, trigger.identityDelta);
    if (!trigger.setFlag.empty()) {
        state.flags.insert(trigger.setFlag);
    }
    if (!trigger.storyNode.empty()) {
        state.storyNodeId = trigger.storyNode;
    }

    return {
        .activated = true,
        .storyNode = trigger.storyNode,
        .message = trigger.id,
    };
}

} // namespace Exo
