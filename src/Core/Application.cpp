#include <ExoEngine/Core/Application.h>

#include <ExoEngine/Assets/ObjImporter.h>
#include <ExoEngine/Core/Logger.h>

#include <SDL2/SDL.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <utility>

namespace Exo {

Application::Application(ApplicationConfig config)
    : config_(std::move(config)),
      scene_(Scene::createReferenceScene()) {}

int Application::run() {
    if (config_.headless) {
        return runHeadless();
    }

    return runWindowed();
}

int Application::runHeadless() {
    Logger::info("ExoEngine headless check");
    Logger::info("Scene loaded: " + scene_.name());
    Logger::info("Fixed camera shots: " + std::to_string(scene_.cameraRig().shots().size()));
    Logger::info("Static mesh slots: " + std::to_string(scene_.staticMeshes().size()));
    Logger::info("Point lights: " + std::to_string(scene_.pointLights().size()));

    ObjImporter importer;
    const std::filesystem::path sampleObj = std::filesystem::path(EXO_ENGINE_ROOT) / "samples" / "reference_room.obj";
    ObjImportResult importResult = importer.importFile(sampleObj);
    for (const AssetDiagnostic& diagnostic : importResult.diagnostics) {
        const std::string line = diagnostic.line == 0 ? "" : ":" + std::to_string(diagnostic.line);
        const std::string message = diagnostic.source.string() + line + " " + diagnostic.message;
        if (diagnostic.severity == AssetDiagnosticSeverity::Error) {
            Logger::error(message);
        } else if (diagnostic.severity == AssetDiagnosticSeverity::Warning) {
            Logger::warn(message);
        } else {
            Logger::info(message);
        }
    }

    if (!importResult.success()) {
        Logger::error("Sample OBJ import failed");
        return 2;
    }

    Logger::info("Sample OBJ imported: "
        + std::to_string(importResult.mesh.vertices.size()) + " vertices, "
        + std::to_string(importResult.mesh.indices.size() / 3) + " triangles, "
        + std::to_string(importResult.mesh.materials.size()) + " materials");
    return 0;
}

int Application::runWindowed() {
    WindowConfig windowConfig;
    windowConfig.title = config_.name;
    windowConfig.width = config_.width;
    windowConfig.height = config_.height;

    if (!window_.create(windowConfig)) {
        return 1;
    }

    if (!renderer_.initialize(window_.width(), window_.height())) {
        return 1;
    }

    Logger::info("Sandbox running. Press Esc to close.");

    bool running = true;
    while (running) {
        SDL_Event event {};
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }

            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                renderer_.resize(
                    static_cast<std::uint32_t>(event.window.data1),
                    static_cast<std::uint32_t>(event.window.data2)
                );
            }
        }

        renderer_.beginFrame(makeCurrentView());
        renderer_.drawReferenceRoom();
        renderer_.endFrame();
        window_.swapBuffers();

        if (config_.maxFrames != 0 && renderer_.stats().frameIndex >= config_.maxFrames) {
            running = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    renderer_.shutdown();
    window_.destroy();
    return 0;
}

RenderView Application::makeCurrentView() const {
    constexpr Vec3 playerProbePosition {0.0f, 0.0f, 0.0f};
    const FixedCameraShot* shot = scene_.cameraRig().chooseShot(playerProbePosition);
    if (shot == nullptr) {
        RenderView fallback;
        fallback.view = Mat4::lookAt({4.6f, 2.35f, 5.2f}, {0.0f, 0.85f, 0.0f}, {0.0f, 1.0f, 0.0f});
        fallback.projection = Mat4::perspective(0.7853981f, 16.0f / 9.0f, 0.05f, 80.0f);
        fallback.cameraPosition = {4.6f, 2.35f, 5.2f};
        return fallback;
    }

    const float aspect = window_.height() == 0
        ? 16.0f / 9.0f
        : static_cast<float>(window_.width()) / static_cast<float>(window_.height());
    return scene_.cameraRig().makeRenderView(*shot, aspect);
}

} // namespace Exo
