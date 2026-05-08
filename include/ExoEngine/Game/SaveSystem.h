#pragma once

#include <filesystem>
#include <string>

#include <ExoEngine/Game/GameState.h>

namespace Exo {

class SaveSystem {
public:
    [[nodiscard]] static bool saveToFile(const std::filesystem::path& path, const GameState& state, std::string& error);
    [[nodiscard]] static bool loadFromFile(const std::filesystem::path& path, GameState& state, std::string& error);
};

} // namespace Exo
