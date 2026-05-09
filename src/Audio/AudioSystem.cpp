#include <ExoEngine/Audio/AudioSystem.h>

#include <ExoEngine/Core/Logger.h>

#include <string>

#ifndef EXO_ENABLE_AUDIO
#define EXO_ENABLE_AUDIO 0
#endif

#if EXO_ENABLE_AUDIO
#include <SDL2/SDL_mixer.h>
#endif

namespace Exo {

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
    Logger::info("SDL_mixer audio initialized");
    return true;
#else
    Logger::warn("Audio disabled at build time");
    return false;
#endif
}

void AudioSystem::shutdown() {
#if EXO_ENABLE_AUDIO
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
    initialized_ = false;
#endif
}

bool AudioSystem::available() const {
    return initialized_;
}

bool AudioSystem::playOneShot(const std::filesystem::path& path) {
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

    if (Mix_PlayChannel(-1, chunk, 0) == -1) {
        Logger::warn(std::string("Unable to play audio cue '") + key + "': " + Mix_GetError());
        return false;
    }

    return true;
#else
    (void)path;
    return false;
#endif
}

} // namespace Exo
