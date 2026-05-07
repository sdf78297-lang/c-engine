#pragma once

#include <cstdint>
#include <string>

#include <ExoEngine/Debug/DebugOverlay.h>
#include <ExoEngine/Platform/Window.h>
#include <ExoEngine/Renderer/Renderer.h>
#include <ExoEngine/Scene/Scene.h>

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
    [[nodiscard]] RenderView makeCurrentView() const;

    ApplicationConfig config_;
    Scene scene_;
    Window window_;
    Renderer renderer_;
    DebugOverlay debugOverlay_;
};

} // namespace Exo
