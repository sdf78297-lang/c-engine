#include <ExoEngine/Engine.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace {

bool hasFlag(std::span<char*> args, std::string_view flag) {
    return std::ranges::any_of(args, [flag](char* arg) {
        return std::string_view(arg) == flag;
    });
}

std::uint32_t integerOption(std::span<char*> args, std::string_view name, std::uint32_t fallback) {
    for (std::size_t i = 1; i + 1 < args.size(); ++i) {
        if (std::string_view(args[i]) != name) {
            continue;
        }

        std::uint32_t value = fallback;
        const std::string_view token(args[i + 1]);
        const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
        if (result.ec == std::errc {} && result.ptr == token.data() + token.size()) {
            return value;
        }
    }

    return fallback;
}

std::string stringOption(std::span<char*> args, std::string_view name, std::string fallback = {}) {
    for (std::size_t i = 1; i + 1 < args.size(); ++i) {
        if (std::string_view(args[i]) == name) {
            return std::string(args[i + 1]);
        }
    }

    return fallback;
}

} // namespace

int main(int argc, char** argv) {
    const std::span<char*> args(argv, static_cast<std::size_t>(argc));

    Exo::ApplicationConfig config;
    config.name = "ExoEngine Survival Horror Sandbox";
    config.width = 1280;
    config.height = 720;
    config.headless = hasFlag(args, "--headless");
    config.maxFrames = integerOption(args, "--frames", 0);
    config.startupRoomId = stringOption(args, "--room");
    config.startupSpawnId = stringOption(args, "--spawn");
    config.showMenu = hasFlag(args, "--menu");
    if (hasFlag(args, "--workstation")) {
        config.showMenu = true;
        config.startupOverlay = "workstation";
    }
    if (hasFlag(args, "--cycle-workstation")) {
        config.showMenu = true;
        config.cycleWorkstationOverlay = true;
    }

    Exo::Application app(config);
    return app.run();
}
