#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <ExoEngine/Audio/AudioSystem.h>
#include <ExoEngine/Debug/DebugOverlay.h>
#include <ExoEngine/Game/CameraDirector.h>
#include <ExoEngine/Game/GameState.h>
#include <ExoEngine/Game/PlayerMotor.h>
#include <ExoEngine/Game/RoomManager.h>
#include <ExoEngine/Platform/Window.h>
#include <ExoEngine/Renderer/Renderer.h>
#include <ExoEngine/Scene/Scene.h>
#include <ExoEngine/Story/StoryRuntime.h>

namespace Exo {

struct ApplicationConfig {
    std::string name = "ExoEngine";
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::uint32_t maxFrames = 0;
    bool headless = false;
    bool showMenu = false;
};

class Application {
public:
    explicit Application(ApplicationConfig config = {});

    int run();

private:
    int runHeadless();
    int runWindowed();
    [[nodiscard]] bool loadStartupScene(bool required);
    [[nodiscard]] bool loadStartupStory(bool required);
    [[nodiscard]] bool loadRoomScene(const std::string& roomId, const std::string& spawnId, bool required);
    [[nodiscard]] RenderView makeCurrentView() const;
    void updatePlayer(float deltaSeconds, float mouseDeltaX, float mouseDeltaY);
    void reloadSceneMeshes();
    void activateCurrentFocus();
    void evaluateCurrentTrigger();
    void processRoomEnterEvents();
    void playAudioCue(const std::string& cueId);
    [[nodiscard]] bool isStaticMeshVisible(const StaticMeshInstance& instance) const;
    void syncStoryToGameState();
    void syncGameStateToStory();
    [[nodiscard]] std::filesystem::path engineRoot() const;
    [[nodiscard]] std::filesystem::path dataRoot() const;
    [[nodiscard]] std::filesystem::path savePath() const;

    ApplicationConfig config_;
    Scene scene_;
    Window window_;
    Renderer renderer_;
    AudioSystem audioSystem_;
    DebugOverlay debugOverlay_;
    StoryRuntime story_;
    RoomManager roomManager_;
    PlayerMotor playerMotor_;
    CameraDirector cameraDirector_;
    GameState gameState_;

    float playerPitch_ = 0.0f;
    std::string currentFocusPrompt_;
    std::vector<std::int32_t> sceneMeshHandles_;
};

} // namespace Exo
