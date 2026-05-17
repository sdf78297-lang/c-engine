#include <ExoEngine/Audio/AudioSystem.h>

#include <ExoEngine/Core/Logger.h>

#include <algorithm>
#include <string>

#ifndef EXO_ENABLE_AUDIO
#define EXO_ENABLE_AUDIO 0
#endif

#if EXO_ENABLE_AUDIO
#include <SDL2/SDL_mixer.h>
#endif

namespace Exo {
namespace {

float clampVolume(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

#if EXO_ENABLE_AUDIO
int mixerVolume(float value) {
    return static_cast<int>(clampVolume(value) * static_cast<float>(MIX_MAX_VOLUME) + 0.5f);
}
#endif

} // namespace

AudioSystem::~AudioSystem() {
    shutdown();
}

bool AudioSystem::initialize() {
#if EXO_ENABLE_AUDIO
    if (initialized_) {
        return true;
    }

    const int requestedCodecs = MIX_INIT_MP3 | MIX_INIT_OGG;
    const int initializedCodecs = Mix_Init(requestedCodecs);
    if ((initializedCodecs & MIX_INIT_MP3) == 0) {
        Logger::warn(std::string("SDL_mixer MP3 codec unavailable: ") + Mix_GetError());
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        Logger::warn(std::string("SDL_mixer audio open failed: ") + Mix_GetError());
        Mix_Quit();
        return false;
    }

    Mix_AllocateChannels(16);
    initialized_ = true;
    applyVolumes();
    Logger::info("SDL_mixer audio initialized");
    return true;
#else
    Logger::warn("Audio disabled at build time");
    return false;
#endif
}

void AudioSystem::shutdown() {
#if EXO_ENABLE_AUDIO
    if (music_ != nullptr) {
        Mix_HaltMusic();
        Mix_FreeMusic(music_);
        music_ = nullptr;
    }
    musicPath_.clear();

    for (auto& [path, chunk] : chunks_) {
        if (chunk != nullptr) {
            Mix_FreeChunk(chunk);
        }
    }
    chunks_.clear();

    if (initialized_) {
        Mix_CloseAudio();
        Mix_Quit();
        initialized_ = false;
    }
#else
    chunks_.clear();
    music_ = nullptr;
    musicPath_.clear();
    initialized_ = false;
#endif
}

bool AudioSystem::available() const {
    return initialized_;
}

bool AudioSystem::playOneShot(const std::filesystem::path& path, int loops) {
#if EXO_ENABLE_AUDIO
    if (!initialized_) {
        return false;
    }

    const std::string key = path.string();
    Mix_Chunk* chunk = nullptr;
    const auto cached = chunks_.find(key);
    if (cached != chunks_.end()) {
        chunk = cached->second;
    } else {
        chunk = Mix_LoadWAV(key.c_str());
        if (chunk == nullptr) {
            Logger::warn(std::string("Unable to load audio cue '") + key + "': " + Mix_GetError());
            return false;
        }
        chunks_.emplace(key, chunk);
    }
    Mix_VolumeChunk(chunk, mixerVolume(masterVolume_));

    if (Mix_PlayChannel(-1, chunk, loops) == -1) {
        Logger::warn(std::string("Unable to play audio cue '") + key + "': " + Mix_GetError());
        return false;
    }

    return true;
#else
    (void)path;
    (void)loops;
    return false;
#endif
}

bool AudioSystem::playMusic(const std::filesystem::path& path, int loops) {
#if EXO_ENABLE_AUDIO
    if (!initialized_) {
        return false;
    }

    const std::string key = path.string();
    if (music_ != nullptr && musicPath_ == key) {
        applyVolumes();
        if (Mix_PlayingMusic() == 0) {
            if (Mix_PlayMusic(music_, loops) != 0) {
                Logger::warn(std::string("Unable to restart music '") + key + "': " + Mix_GetError());
                return false;
            }
        }
        return true;
    }

    if (music_ != nullptr) {
        Mix_HaltMusic();
        Mix_FreeMusic(music_);
        music_ = nullptr;
        musicPath_.clear();
    }

    music_ = Mix_LoadMUS(key.c_str());
    if (music_ == nullptr) {
        Logger::warn(std::string("Unable to load music '") + key + "': " + Mix_GetError());
        return false;
    }

    musicPath_ = key;
    applyVolumes();
    if (Mix_PlayMusic(music_, loops) != 0) {
        Logger::warn(std::string("Unable to play music '") + key + "': " + Mix_GetError());
        return false;
    }

    return true;
#else
    (void)path;
    (void)loops;
    return false;
#endif
}

void AudioSystem::stopAllCues() {
#if EXO_ENABLE_AUDIO
    if (initialized_) {
        Mix_HaltChannel(-1);
    }
#endif
}

void AudioSystem::stopMusic() {
#if EXO_ENABLE_AUDIO
    if (initialized_) {
        Mix_HaltMusic();
    }
#endif
}

void AudioSystem::setMasterVolume(float value) {
    masterVolume_ = clampVolume(value);
    applyVolumes();
}

void AudioSystem::setMusicVolume(float value) {
    musicVolume_ = clampVolume(value);
    applyVolumes();
}

float AudioSystem::masterVolume() const {
    return masterVolume_;
}

float AudioSystem::musicVolume() const {
    return musicVolume_;
}

void AudioSystem::applyVolumes() {
#if EXO_ENABLE_AUDIO
    if (!initialized_) {
        return;
    }
    Mix_Volume(-1, mixerVolume(masterVolume_));
    Mix_VolumeMusic(mixerVolume(masterVolume_ * musicVolume_));
#endif
}

} // namespace Exo
