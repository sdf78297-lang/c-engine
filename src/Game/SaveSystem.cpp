#include <ExoEngine/Game/SaveSystem.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>

namespace Exo {
namespace {

using Json = nlohmann::json;

Json vec3ToJson(Vec3 value) {
    return Json::array({value.x, value.y, value.z});
}

Vec3 vec3FromJson(const Json& value, Vec3 fallback) {
    if (!value.is_array() || value.size() != 3) {
        return fallback;
    }
    if (!value[0].is_number() || !value[1].is_number() || !value[2].is_number()) {
        return fallback;
    }
    return {
        value[0].get<float>(),
        value[1].get<float>(),
        value[2].get<float>(),
    };
}

Json stringSetToJson(const std::unordered_set<std::string>& values) {
    Json array = Json::array();
    for (const std::string& value : values) {
        array.push_back(value);
    }
    return array;
}

std::unordered_set<std::string> stringSetFromJson(const Json& value) {
    std::unordered_set<std::string> result;
    if (!value.is_array()) {
        return result;
    }
    for (const Json& entry : value) {
        if (entry.is_string()) {
            result.insert(entry.get<std::string>());
        }
    }
    return result;
}

} // namespace

bool SaveSystem::saveToFile(const std::filesystem::path& path, const GameState& state, std::string& error) {
    error.clear();
    try {
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        Json root;
        root["version"] = 1;
        root["roomId"] = state.roomId;
        root["spawnId"] = state.spawnId;
        root["storyNodeId"] = state.storyNodeId;
        root["playerPosition"] = vec3ToJson(state.playerPosition);
        root["playerYaw"] = state.playerYaw;
        root["identity"] = state.identity;
        root["inventory"] = stringSetToJson(state.inventory);
        root["flags"] = stringSetToJson(state.flags);

        std::ofstream output(path, std::ios::out | std::ios::binary);
        if (!output) {
            error = "Unable to write save file: " + path.string();
            return false;
        }
        output << root.dump(2) << "\n";
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    return true;
}

bool SaveSystem::loadFromFile(const std::filesystem::path& path, GameState& state, std::string& error) {
    error.clear();

    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input) {
        error = "Unable to open save file: " + path.string();
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    try {
        const Json root = Json::parse(buffer.str());
        if (root.contains("roomId") && root["roomId"].is_string()) {
            state.roomId = root["roomId"].get<std::string>();
        }
        if (root.contains("spawnId") && root["spawnId"].is_string()) {
            state.spawnId = root["spawnId"].get<std::string>();
        }
        if (root.contains("storyNodeId") && root["storyNodeId"].is_string()) {
            state.storyNodeId = root["storyNodeId"].get<std::string>();
        }
        if (root.contains("playerPosition")) {
            state.playerPosition = vec3FromJson(root["playerPosition"], state.playerPosition);
        }
        if (root.contains("playerYaw") && root["playerYaw"].is_number()) {
            state.playerYaw = root["playerYaw"].get<float>();
        }
        if (root.contains("identity") && root["identity"].is_number_integer()) {
            state.identity = root["identity"].get<int>();
        }
        if (root.contains("inventory")) {
            state.inventory = stringSetFromJson(root["inventory"]);
        }
        if (root.contains("flags")) {
            state.flags = stringSetFromJson(root["flags"]);
        }
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }

    return true;
}

} // namespace Exo
