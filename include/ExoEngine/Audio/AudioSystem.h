#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

struct Mix_Chunk;
struct Mix_Music;

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
    [[nodiscard]] bool playMusic(const std::filesystem::path& path, int loops = -1);
    void stopMusic();
    void setMasterVolume(float value);
    void setMusicVolume(float value);
    [[nodiscard]] float masterVolume() const;
    [[nodiscard]] float musicVolume() const;

private:
    void applyVolumes();

    std::unordered_map<std::string, Mix_Chunk*> chunks_;
    Mix_Music* music_ = nullptr;
    std::string musicPath_;
    float masterVolume_ = 0.55f;
    float musicVolume_ = 0.24f;
    bool initialized_ = false;
};

} // namespace Exo
