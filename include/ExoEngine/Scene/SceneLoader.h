#pragma once

#include <filesystem>
#include <string_view>

#include <ExoEngine/Scene/Scene.h>

namespace Exo {

class SceneLoader {
public:
    [[nodiscard]] static Scene loadFromFile(const std::filesystem::path& path);
    [[nodiscard]] static Scene loadFromJsonString(std::string_view source, std::string_view sourceName = "<memory>");
};

} // namespace Exo
