#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <ExoEngine/Debug/DebugOverlay.h>
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
    [[nodiscard]] RenderView makeCurrentView() const;
    void updatePlayer(float deltaSeconds, float mouseDeltaX, float mouseDeltaY);

    ApplicationConfig config_;
    Scene scene_;
    Window window_;
    Renderer renderer_;
    DebugOverlay debugOverlay_;
    StoryRuntime story_;

    Vec3 playerPosition_ {0.0f, 1.65f, 1.5f};
    float playerYaw_ = 0.0f;
    float playerPitch_ = 0.0f;
    std::vector<std::int32_t> sceneMeshHandles_;
};

} // namespace Exo
