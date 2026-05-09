#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

struct Mix_Chunk;

namespace Exo {

class AudioSystem {
public:
    AudioSystem() = default;
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    [[nodiscard]] bool initialize();
    void shutdown();
    [[nodiscard]] bool available() const;
    [[nodiscard]] bool playOneShot(const std::filesystem::path& path);

private:
    std::unordered_map<std::string, Mix_Chunk*> chunks_;
    bool initialized_ = false;
};

} // namespace Exo
