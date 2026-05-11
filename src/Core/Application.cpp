#include <ExoEngine/Core/Application.h>

#include <ExoEngine/Assets/GltfLoader.h>
#include <ExoEngine/Assets/ObjImporter.h>
#include <ExoEngine/Core/Logger.h>
#include <ExoEngine/Game/InteractionSystem.h>
#include <ExoEngine/Game/InventorySystem.h>
#include <ExoEngine/Game/SaveSystem.h>
#include <ExoEngine/Scene/SceneLoader.h>
#include <ExoEngine/UI/HtmlMenu.h>

#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace Exo {

namespace {

std::string formatVec3(Vec3 v, int precision = 3) {
    std::ostringstream s;
    s << std::fixed << std::setprecision(precision)
      << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return s.str();
}

std::string formatSize(Vec3 a, Vec3 b) {
    std::ostringstream s;
    s << std::fixed << std::setprecision(3)
      << (b.x - a.x) << " x " << (b.y - a.y) << " x " << (b.z - a.z);
    return s.str();
}

std::string lowerExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

void transformAabb(Vec3 localMin, Vec3 localMax, const Transform& t, Vec3& outMin, Vec3& outMax) {
    constexpr float kInf = std::numeric_limits<float>::infinity();
    outMin = {kInf, kInf, kInf};
    outMax = {-kInf, -kInf, -kInf};

    const float c = std::cos(t.rotation.y);
    const float s = std::sin(t.rotation.y);

    const std::array<Vec3, 8> corners {{
        {localMin.x, localMin.y, localMin.z},
        {localMax.x, localMin.y, localMin.z},
        {localMax.x, localMax.y, localMin.z},
        {localMin.x, localMax.y, localMin.z},
        {localMin.x, localMin.y, localMax.z},
        {localMax.x, localMin.y, localMax.z},
        {localMax.x, localMax.y, localMax.z},
        {localMin.x, localMax.y, localMax.z},
    }};

    for (Vec3 corner : corners) {
        // Apply scale
        corner.x *= t.scale.x;
        corner.y *= t.scale.y;
        corner.z *= t.scale.z;
        // Apply rotateY (matches Mat4::rotateY: x' = c*x + s*z, z' = -s*x + c*z)
        const Vec3 rotated {
            c * corner.x + s * corner.z,
            corner.y,
            -s * corner.x + c * corner.z,
        };
        // Apply translation
        const Vec3 world {
            rotated.x + t.position.x,
            rotated.y + t.position.y,
            rotated.z + t.position.z,
        };
        if (world.x < outMin.x) outMin.x = world.x;
        if (world.y < outMin.y) outMin.y = world.y;
        if (world.z < outMin.z) outMin.z = world.z;
        if (world.x > outMax.x) outMax.x = world.x;
        if (world.y > outMax.y) outMax.y = world.y;
        if (world.z > outMax.z) outMax.z = world.z;
    }
}

bool overlapsExpandedXz(Vec3 position, const Bounds3& bounds, float radius) {
    return position.x >= (bounds.min.x - radius) && position.x <= (bounds.max.x + radius)
        && position.z >= (bounds.min.z - radius) && position.z <= (bounds.max.z + radius);
}

float smoothStep01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float lerpFloat(float a, float b, float t) {
    return a + (b - a) * t;
}

Vec3 lerpVec3(Vec3 a, Vec3 b, float t) {
    return {
        lerpFloat(a.x, b.x, t),
        lerpFloat(a.y, b.y, t),
        lerpFloat(a.z, b.z, t),
    };
}

float distanceXz(Vec3 a, Vec3 b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt((dx * dx) + (dz * dz));
}

bool blockedByRoomCollider(Vec3 position, const RoomDefinition& room, float radius) {
    for (const RoomCollisionBox& collider : room.collisionBoxes) {
        if (overlapsExpandedXz(position, collider.bounds, radius)) {
            return true;
        }
    }
    return false;
}

} // namespace

Application::Application(ApplicationConfig config)
    : config_(std::move(config)),
      scene_(Scene::createReferenceScene()) {
    for (const std::string& flag : config_.startupFlags) {
        if (!flag.empty()) {
            gameState_.flags.insert(flag);
        }
    }
    if (!config_.startupRoomId.empty()) {
        gameState_.roomId = config_.startupRoomId;
        gameState_.spawnId = config_.startupSpawnId;
    } else if (!config_.startupSpawnId.empty()) {
        gameState_.spawnId = config_.startupSpawnId;
    }
}

int Application::run() {
    if (config_.headless) {
        return runHeadless();
    }

    return runWindowed();
}

int Application::runHeadless() {
    Logger::info("ExoEngine headless check");
    if (!loadStartupScene(true)) {
        return 3;
    }
    if (!loadStartupStory(true)) {
        return 4;
    }

    Logger::info("Scene loaded: " + scene_.name());
    Logger::info("Story loaded: " + story_.title()
        + " (" + std::to_string(story_.nodes().size()) + " nodes, identity "
        + std::to_string(story_.identity()) + "%)");
    Logger::info("Fixed camera shots: " + std::to_string(scene_.cameraRig().shots().size()));
    Logger::info("Static mesh slots: " + std::to_string(scene_.staticMeshes().size()));
    Logger::info("Point lights: " + std::to_string(scene_.pointLights().size()));
    if (roomManager_.loaded()) {
        Logger::info("Gameplay room loaded: " + roomManager_.currentRoom().id
            + " (" + std::to_string(roomManager_.currentRoom().interactions.size()) + " interactions, "
            + std::to_string(roomManager_.currentRoom().doors.size()) + " doors, "
            + std::to_string(roomManager_.currentRoom().triggers.size()) + " triggers, "
            + std::to_string(roomManager_.currentRoom().collisionBoxes.size()) + " collision boxes)");
    }

    ObjImporter importer;
    const std::array<std::filesystem::path, 2> sampleObjs {
        std::filesystem::path(EXO_ENGINE_ROOT) / "samples" / "reference_room.obj",
        std::filesystem::path(EXO_ENGINE_ROOT) / "samples" / "character.obj",
    };

    for (const std::filesystem::path& sampleObj : sampleObjs) {
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
            Logger::error("Sample OBJ import failed: " + sampleObj.string());
            return 2;
        }

        Logger::info("Sample OBJ imported (" + sampleObj.filename().string() + "): "
            + std::to_string(importResult.mesh.vertices.size()) + " vertices, "
            + std::to_string(importResult.mesh.indices.size() / 3) + " triangles, "
            + std::to_string(importResult.mesh.materials.size()) + " materials");
    }

    Logger::info("=== Scene info ===");
    Logger::info("Engine root: " + std::string(EXO_ENGINE_ROOT));
    Logger::info("Reference room AABB (world): min=(-2.000, 0.000, -2.000) max=(2.000, 2.400, 2.000) size=4.000 x 2.400 x 4.000");
    Logger::info("Player spawn: pos=" + formatVec3(gameState_.playerPosition)
        + " yaw=" + std::to_string(gameState_.playerYaw)
        + " pitch=" + std::to_string(playerPitch_)
        + " (eye height 1.65 m)");
    if (roomManager_.loaded()) {
        const Bounds3& walkBounds = roomManager_.currentRoom().walkBounds;
        Logger::info("Walkable bounds: min=" + formatVec3(walkBounds.min) + " max=" + formatVec3(walkBounds.max));
    }

    const std::filesystem::path engineRoot(EXO_ENGINE_ROOT);
    Logger::info("Static meshes (" + std::to_string(scene_.staticMeshes().size()) + "):");
    for (std::size_t i = 0; i < scene_.staticMeshes().size(); ++i) {
        const StaticMeshInstance& m = scene_.staticMeshes()[i];
        Logger::info("  [" + std::to_string(i) + "] " + m.name
            + " pos=" + formatVec3(m.transform.position)
            + " rotY=" + std::to_string(m.transform.rotation.y)
            + " scale=" + formatVec3(m.transform.scale));
        if (m.meshSource.empty()) {
            Logger::warn("       no meshSource; scene instance will not render");
            continue;
        }
        std::filesystem::path source(m.meshSource);
        if (!source.is_absolute()) {
            source = engineRoot / source;
        }
        const std::string ext = lowerExtension(source);
        if (ext == ".obj") {
            ObjImportResult importResult = importer.importFile(source);
            if (!importResult.success()) {
                Logger::error("       OBJ load failed: " + source.string());
                continue;
            }
            Logger::info("       OBJ: " + m.meshSource
                + " (" + std::to_string(importResult.mesh.vertices.size()) + " verts, "
                + std::to_string(importResult.mesh.indices.size() / 3) + " tris)");
            if (importResult.mesh.bounds.valid) {
                Logger::info("       local AABB: min=" + formatVec3(importResult.mesh.bounds.min)
                    + " max=" + formatVec3(importResult.mesh.bounds.max)
                    + " size=" + formatSize(importResult.mesh.bounds.min, importResult.mesh.bounds.max));
                Vec3 worldMin, worldMax;
                transformAabb(importResult.mesh.bounds.min, importResult.mesh.bounds.max, m.transform, worldMin, worldMax);
                Logger::info("       world AABB: min=" + formatVec3(worldMin)
                    + " max=" + formatVec3(worldMax)
                    + " size=" + formatSize(worldMin, worldMax));
            }
        } else if (ext == ".glb" || ext == ".gltf") {
            try {
                GltfModelData data = GltfLoader::loadFromFile(source);
                if (data.primitives.empty()) {
                    Logger::warn("       glTF empty: " + source.string());
                    continue;
                }
                Logger::info("       glTF: " + m.meshSource
                    + " (" + std::to_string(data.primitives.size()) + " prims)");
                Logger::info("       local AABB: min=" + formatVec3(data.aabbMin)
                    + " max=" + formatVec3(data.aabbMax)
                    + " size=" + formatSize(data.aabbMin, data.aabbMax));
                Vec3 worldMin, worldMax;
                transformAabb(data.aabbMin, data.aabbMax, m.transform, worldMin, worldMax);
                Logger::info("       world AABB: min=" + formatVec3(worldMin)
                    + " max=" + formatVec3(worldMax)
                    + " size=" + formatSize(worldMin, worldMax));
            } catch (const std::exception& e) {
                Logger::error(std::string("       glTF load failed: ") + e.what());
            }
        } else {
            Logger::error("       unsupported meshSource format: " + source.string());
        }
    }

    Logger::info("Point lights (" + std::to_string(scene_.pointLights().size()) + "):");
    for (std::size_t i = 0; i < scene_.pointLights().size(); ++i) {
        const PointLight& l = scene_.pointLights()[i];
        Logger::info("  [" + std::to_string(i) + "] pos=" + formatVec3(l.position)
            + " color=" + formatVec3(l.color)
            + " radius=" + std::to_string(l.radius)
            + " intensity=" + std::to_string(l.intensity));
    }

    return 0;
}

int Application::runWindowed() {
    if (!loadStartupScene(false)) {
        Logger::warn("Using built-in reference scene");
    }
    if (!loadStartupStory(false)) {
        Logger::warn("Story overlay is not available");
    }

    WindowConfig windowConfig;
    windowConfig.title = config_.name;
    windowConfig.width = config_.width;
    windowConfig.height = config_.height;

    if (!window_.create(windowConfig)) {
        return 1;
    }

    const bool audioReady = audioSystem_.initialize();
    if (!audioReady) {
        Logger::warn("Audio system unavailable; continuing without voice cues");
    } else {
        audioSystem_.setMasterVolume(0.55f);
        audioSystem_.setMusicVolume(0.24f);
    }

    if (!renderer_.initialize(window_.width(), window_.height())) {
        audioSystem_.shutdown();
        return 1;
    }

    if (!debugOverlay_.initialize(window_.nativeHandle(), nullptr)) {
        Logger::warn("Gameplay HUD overlay unavailable; interaction prompt will be hidden");
    }

    reloadSceneMeshes();

    HtmlMenu htmlMenu;
    enum class HtmlOverlayKind {
        None,
        MainMenu,
        PauseSettings,
        Workstation,
    };

    HtmlOverlayKind htmlOverlayKind = HtmlOverlayKind::None;
    bool htmlMenuActive = false;
    bool gamePaused = false;
    bool cycleWorkstationOpened = false;
    const auto closeHtmlOverlay = [&](bool captureMouse) {
        htmlMenu.shutdown();
        htmlMenuActive = false;
        gamePaused = false;
        htmlOverlayKind = HtmlOverlayKind::None;
        SDL_StopTextInput();
        if (captureMouse && config_.maxFrames == 0) {
            SDL_SetRelativeMouseMode(SDL_TRUE);
        }
    };
    const auto openHtmlOverlay = [&](
        const std::filesystem::path& htmlPath,
        const std::string& initialScreen,
        bool pauseOverlay,
        HtmlOverlayKind kind) {
        if (htmlMenu.isActive()) {
            htmlMenu.shutdown();
            SDL_StopTextInput();
        }
        htmlMenu.clearRequests();
        htmlMenuActive = htmlMenu.initialize({
            .engineRoot = engineRoot(),
            .htmlPath = htmlPath,
            .ultralightResourcePath = engineRoot() / "local_deps" / "ultralight-sdk" / "resources",
            .initialScreen = initialScreen,
            .pauseOverlay = pauseOverlay,
            .width = window_.width(),
            .height = window_.height(),
        });
        htmlOverlayKind = htmlMenuActive ? kind : HtmlOverlayKind::None;
        gamePaused = pauseOverlay && htmlMenuActive;
        if (htmlMenuActive) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_StartTextInput();
        } else {
            SDL_StopTextInput();
        }
        return htmlMenuActive;
    };
    const auto openHtmlMenu = [&](const std::string& initialScreen, bool pauseOverlay) {
        return openHtmlOverlay(
            std::filesystem::path("assets") / "ui" / "main_menu" / "web" / "index.html",
            initialScreen,
            pauseOverlay,
            pauseOverlay ? HtmlOverlayKind::PauseSettings : HtmlOverlayKind::MainMenu);
    };
    const auto openWorkstation = [&]() {
        return openHtmlOverlay(
            std::filesystem::path("assets") / "ui" / "workstation" / "web" / "index.html",
            "",
            true,
            HtmlOverlayKind::Workstation);
    };

    if (config_.maxFrames == 0 || config_.showMenu) {
        if (config_.startupOverlay == "workstation") {
            openWorkstation();
        } else {
            openHtmlMenu("screen-menu", false);
        }
    }

    if (config_.maxFrames == 0 && !htmlMenuActive) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    } else {
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
    if (!htmlMenuActive) {
        processRoomEnterEvents();
    }

    Logger::info("Runtime running. HTML menu: mouse/Enter start, Esc back. Game: WASD move, mouse look, Shift run, Tab mouse capture, E interact, N skip to collapse, F5 save, F9 load, Esc settings, Q temporary quit.");

    bool running = true;
    auto lastTime = SDL_GetPerformanceCounter();
    const float perfFreq = static_cast<float>(SDL_GetPerformanceFrequency());
    const auto frameLoopStart = lastTime;
    double menuUpdateSeconds = 0.0;
    double menuRenderSeconds = 0.0;
    double menuSwapSeconds = 0.0;
    double menuUpdateMaxSeconds = 0.0;
    double menuRenderMaxSeconds = 0.0;
    double menuSwapMaxSeconds = 0.0;
    std::uint64_t menuProfileFrames = 0;

    const auto ticksToSeconds = [](std::uint64_t ticks) {
        return static_cast<double>(ticks) / static_cast<double>(SDL_GetPerformanceFrequency());
    };

    while (running) {
        const auto now = SDL_GetPerformanceCounter();
        const float deltaSeconds = static_cast<float>(now - lastTime) / perfFreq;
        lastTime = now;
        const float frameDeltaSeconds = std::clamp(deltaSeconds, 0.0f, 0.05f);
        visualTime_ += frameDeltaSeconds;

        float mouseDeltaX = 0.0f;
        float mouseDeltaY = 0.0f;

        SDL_Event event {};
        while (SDL_PollEvent(&event) != 0) {
            if (htmlMenuActive) {
                htmlMenu.handleEvent(event);
            } else if (debugOverlay_.isInitialized()) {
                debugOverlay_.handleEvent(event);
            }

            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.repeat == 0
                        && (event.key.keysym.sym == SDLK_n || event.key.keysym.scancode == SDL_SCANCODE_N)
                        && (!htmlMenuActive || htmlOverlayKind != HtmlOverlayKind::Workstation)) {
                        if (htmlMenuActive) {
                            closeHtmlOverlay(true);
                        }
                        skipToCollapseShortcut();
                        break;
                    }
                    if (htmlMenuActive) {
                        break;
                    }
                    if (workstationSequenceBlocksPlayer() || collapseSequenceBlocksPlayer() || sequenceBlocksPlayer()) {
                        if (event.key.keysym.sym == SDLK_q) {
                            running = false;
                        }
                        break;
                    }
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        if (!openHtmlMenu("screen-settings", true)) {
                            Logger::warn("Pause settings menu unavailable");
                        }
                    } else if (event.key.keysym.sym == SDLK_q) {
                        running = false;
                    } else if (event.key.keysym.sym == SDLK_TAB) {
                        const SDL_bool wasRelative = SDL_GetRelativeMouseMode();
                        SDL_SetRelativeMouseMode(wasRelative == SDL_TRUE ? SDL_FALSE : SDL_TRUE);
                    } else if (event.key.keysym.sym == SDLK_e) {
                        activateCurrentFocus();
                    } else if (event.key.keysym.sym == SDLK_F5) {
                        syncStoryToGameState();
                        std::string error;
                        if (SaveSystem::saveToFile(savePath(), gameState_, error)) {
                            Logger::info("Saved game state: " + savePath().string());
                        } else {
                            Logger::error(error);
                        }
                    } else if (event.key.keysym.sym == SDLK_F9) {
                        std::string error;
                        if (SaveSystem::loadFromFile(savePath(), gameState_, error)) {
                            const GameState loadedState = gameState_;
                            if (loadRoomScene(loadedState.roomId, loadedState.spawnId, false)) {
                                gameState_ = loadedState;
                                syncGameStateToStory();
                                playRoomMusic();
                                Logger::info("Loaded game state: " + savePath().string());
                            }
                        } else {
                            Logger::error(error);
                        }
                    }
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        window_.resize(
                            static_cast<std::uint32_t>(event.window.data1),
                            static_cast<std::uint32_t>(event.window.data2)
                        );
                        renderer_.resize(
                            static_cast<std::uint32_t>(event.window.data1),
                            static_cast<std::uint32_t>(event.window.data2)
                        );
                        if (htmlMenuActive) {
                            htmlMenu.resize(
                                static_cast<std::uint32_t>(event.window.data1),
                                static_cast<std::uint32_t>(event.window.data2)
                            );
                        }
                    }
                    break;
                case SDL_MOUSEMOTION:
                    if (!htmlMenuActive && SDL_GetRelativeMouseMode() == SDL_TRUE) {
                        mouseDeltaX += static_cast<float>(event.motion.xrel);
                        mouseDeltaY += static_cast<float>(event.motion.yrel);
                    }
                    break;
                default:
                    break;
            }
        }

        if (updateWorkstationSequence(deltaSeconds) && !htmlMenuActive) {
            if (openWorkstation()) {
                gameState_.flags.insert("flag_workstation_opened");
                Logger::info("Opened UI overlay: workstation");
                continue;
            }
            workstationSequenceState_ = WorkstationSequenceState::Exiting;
            workstationSequenceTimer_ = 0.0f;
            Logger::warn("Workstation UI overlay unavailable");
        }

        if (!pendingUiOverlay_.empty() && !htmlMenuActive) {
            const std::string overlay = std::exchange(pendingUiOverlay_, {});
            if (overlay == "workstation") {
                if (openWorkstation()) {
                    gameState_.flags.insert("flag_workstation_opened");
                    Logger::info("Opened UI overlay: workstation");
                    continue;
                }
                Logger::warn("Workstation UI overlay unavailable");
            } else {
                Logger::warn("Unknown UI overlay requested: " + overlay);
            }
        }

        if (htmlMenuActive) {
            const auto menuUpdateStart = SDL_GetPerformanceCounter();
            htmlMenu.update();
            const auto menuUpdateEnd = SDL_GetPerformanceCounter();
            if (config_.cycleWorkstationOverlay
                && !cycleWorkstationOpened
                && htmlOverlayKind == HtmlOverlayKind::MainMenu
                && renderer_.stats().frameIndex > 12) {
                closeHtmlOverlay(false);
                cycleWorkstationOpened = true;
                openWorkstation();
                continue;
            }
            if (auto setting = htmlMenu.takeSettingChange()) {
                if (setting->key == "vol-master") {
                    audioSystem_.setMasterVolume(setting->value);
                } else if (setting->key == "vol-music") {
                    audioSystem_.setMusicVolume(setting->value);
                }
            }

            if (htmlOverlayKind == HtmlOverlayKind::Workstation) {
                if (htmlMenu.takeWorkstationSaveRequested()) {
                    gameState_.flags.insert("flag_workstation_saved");
                    Logger::info("Workstation save requested");
                }
                if (htmlMenu.takeWorkstationSendRequested()) {
                    gameState_.flags.insert("flag_workstation_send_failed");
                    Logger::info("Workstation send requested");
                }
                if (htmlMenu.takeWorkstationCompleteRequested()) {
                    gameState_.flags.insert("flag_workstation_task_complete");
                    gameState_.flags.insert("office_report_completed");
                    Logger::info("Workstation task completed");
                }
                const bool shutdownRequested = htmlMenu.takeWorkstationShutdownRequested();
                if (shutdownRequested || htmlMenu.closeRequested()) {
                    if (shutdownRequested) {
                        gameState_.flags.insert("office_pc_shutdown");
                    }
                    closeHtmlOverlay(false);
                    beginWorkstationExitSequence();
                    Logger::info(shutdownRequested
                        ? "Workstation shutdown requested"
                        : "Closed UI overlay: workstation");
                    continue;
                }
            }

            if (htmlOverlayKind == HtmlOverlayKind::MainMenu && htmlMenu.startRequested()) {
                closeHtmlOverlay(true);
                processRoomEnterEvents();
                Logger::info("HTML menu requested game start");
                continue;
            }
            if (htmlOverlayKind == HtmlOverlayKind::PauseSettings && htmlMenu.resumeRequested()) {
                closeHtmlOverlay(true);
                Logger::info("HTML menu requested game resume");
                continue;
            }
            if (htmlMenu.quitRequested()) {
                running = false;
            }

            renderer_.beginFrame(makeCurrentView());
            htmlMenu.render();
            renderer_.endFrame();
            const auto menuRenderEnd = SDL_GetPerformanceCounter();
            window_.swapBuffers();
            const auto menuSwapEnd = SDL_GetPerformanceCounter();

            if (config_.maxFrames == 0) {
                constexpr double targetMenuFrameSeconds = 1.0 / 45.0;
                const double menuFrameSeconds = ticksToSeconds(menuSwapEnd - now);
                if (menuFrameSeconds < targetMenuFrameSeconds) {
                    const auto delayMs = static_cast<std::uint32_t>(
                        (targetMenuFrameSeconds - menuFrameSeconds) * 1000.0);
                    if (delayMs > 0) {
                        SDL_Delay(delayMs);
                    }
                }
            }

            if (config_.maxFrames != 0) {
                const double updateSeconds = ticksToSeconds(menuUpdateEnd - menuUpdateStart);
                const double renderSeconds = ticksToSeconds(menuRenderEnd - menuUpdateEnd);
                const double swapSeconds = ticksToSeconds(menuSwapEnd - menuRenderEnd);
                menuUpdateSeconds += updateSeconds;
                menuRenderSeconds += renderSeconds;
                menuSwapSeconds += swapSeconds;
                menuUpdateMaxSeconds = std::max(menuUpdateMaxSeconds, updateSeconds);
                menuRenderMaxSeconds = std::max(menuRenderMaxSeconds, renderSeconds);
                menuSwapMaxSeconds = std::max(menuSwapMaxSeconds, swapSeconds);
                ++menuProfileFrames;
            }

            if (config_.maxFrames != 0 && renderer_.stats().frameIndex >= config_.maxFrames) {
                running = false;
            }

            std::this_thread::yield();
            continue;
        }

        updateCollapseSequence(deltaSeconds);
        updateSequenceRuntime(deltaSeconds);

        const bool gameplayControlBlocked = workstationSequenceBlocksPlayer()
            || collapseSequenceBlocksPlayer()
            || sequenceBlocksPlayer();
        if (config_.maxFrames == 0
            && !gamePaused
            && (!gameplayControlBlocked || lyingLimitedLookActive())) {
            updatePlayer(deltaSeconds, mouseDeltaX, mouseDeltaY);
            if (!gameplayControlBlocked) {
                evaluateCurrentTrigger();
            }
        }

        std::vector<RenderPointLight> renderLights;
        renderLights.reserve(scene_.pointLights().size());
        for (std::size_t lightIndex = 0; lightIndex < scene_.pointLights().size(); ++lightIndex) {
            const PointLight& light = scene_.pointLights()[lightIndex];
            const float phase = static_cast<float>(lightIndex) * 1.713f;
            const float lightBreath = std::sin((visualTime_ * 2.1f) + phase) * 0.006f;
            const float electricalNoise = std::sin((visualTime_ * 8.7f) + (phase * 0.63f)) * 0.003f;
            const float verticalDrift = std::sin((visualTime_ * 5.4f) + phase) * 0.002f;
            RenderPointLight renderLight {
                .position = light.position,
                .color = light.color,
                .radius = light.radius,
                .intensity = light.intensity,
            };
            renderLight.position.y += verticalDrift;
            renderLight.intensity *= std::clamp(1.0f + lightBreath + electricalNoise, 0.96f, 1.04f);
            renderLights.push_back(renderLight);
        }
        renderer_.setPointLights(renderLights);
        renderer_.setEnvironment(scene_.renderEnvironment());
        renderer_.setScreenOverlay(currentScreenOverlay());
        renderer_.beginFrame(makeCurrentView());
        for (std::size_t i = 0; i < scene_.staticMeshes().size(); ++i) {
            const StaticMeshInstance& instance = scene_.staticMeshes()[i];
            if (!isStaticMeshVisible(instance)) {
                continue;
            }
            const Transform renderTransform = animatedStaticMeshTransform(instance);
            const Mat4 model = Mat4::translate(renderTransform.position)
                * Mat4::rotateY(renderTransform.rotation.y)
                * Mat4::rotateX(renderTransform.rotation.x)
                * Mat4::rotateZ(renderTransform.rotation.z)
                * Mat4::scale(renderTransform.scale);
            if (sceneMeshHandles_[i] >= 0) {
                renderer_.drawSceneMesh(
                    sceneMeshHandles_[i],
                    model,
                    instance.materialOverride,
                    animationSystem_.jointMatricesFor(instance.name));
            }
        }
        renderer_.endFrame();
        if (debugOverlay_.isInitialized()) {
            debugOverlay_.beginFrame();
            debugOverlay_.drawInteractionPrompt(
                currentFocusInteractionId_,
                workstationSequenceBlocksPlayer(),
                gameState_.flags.contains("flag_workstation_task_complete"));
            debugOverlay_.endFrame();
        }
        window_.swapBuffers();

        if (config_.maxFrames != 0 && renderer_.stats().frameIndex >= config_.maxFrames) {
            running = false;
        }

        std::this_thread::yield();
    }

    SDL_SetRelativeMouseMode(SDL_FALSE);

    if (config_.maxFrames != 0) {
        const auto frameLoopEnd = SDL_GetPerformanceCounter();
        const double elapsedSeconds = static_cast<double>(frameLoopEnd - frameLoopStart)
            / static_cast<double>(SDL_GetPerformanceFrequency());
        const double averageFps = elapsedSeconds > 0.0
            ? static_cast<double>(renderer_.stats().frameIndex) / elapsedSeconds
            : 0.0;
        Logger::info("Frame run: " + std::to_string(renderer_.stats().frameIndex)
            + " frames in " + std::to_string(elapsedSeconds)
            + " sec (" + std::to_string(averageFps) + " fps)");
        if (menuProfileFrames != 0) {
            const double frames = static_cast<double>(menuProfileFrames);
            Logger::info("Menu profile avg/max ms: update "
                + std::to_string((menuUpdateSeconds / frames) * 1000.0)
                + "/" + std::to_string(menuUpdateMaxSeconds * 1000.0)
                + ", render " + std::to_string((menuRenderSeconds / frames) * 1000.0)
                + "/" + std::to_string(menuRenderMaxSeconds * 1000.0)
                + ", swap " + std::to_string((menuSwapSeconds / frames) * 1000.0)
                + "/" + std::to_string(menuSwapMaxSeconds * 1000.0));
        }
    }

    debugOverlay_.shutdown();
    renderer_.shutdown();
    audioSystem_.shutdown();
    window_.destroy();
    return 0;
}

void Application::updatePlayer(float deltaSeconds, float mouseDeltaX, float mouseDeltaY) {
    constexpr float kMouseSensitivity = 0.0022f;
    constexpr float kMaxPitch = 1.35f;
    constexpr float kWalkSpeed = 2.2f;
    constexpr float kRunMultiplier = 1.65f;
    constexpr float kEyeHeight = 1.65f;
    const float dt = std::clamp(deltaSeconds, 0.0f, 0.05f);

    if (lyingLimitedLookActive()) {
        const float settle = 1.0f - std::exp(-dt * 8.0f);
        walkCameraAmount_ = lerpFloat(walkCameraAmount_, 0.0f, settle);
        updateLyingLimitedLook(deltaSeconds, mouseDeltaX, mouseDeltaY);
        return;
    }

    const Vec3 frameStartPosition = gameState_.playerPosition;
    const float control = collapseControlMultiplier();

    gameState_.playerYaw += mouseDeltaX * kMouseSensitivity * control;
    playerPitch_ -= mouseDeltaY * kMouseSensitivity * control;
    playerPitch_ = std::clamp(playerPitch_, -kMaxPitch, kMaxPitch);

    const std::uint8_t* keys = SDL_GetKeyboardState(nullptr);
    if (keys == nullptr) {
        return;
    }

    Vec3 move {0.0f, 0.0f, 0.0f};
    const float sinYaw = std::sin(gameState_.playerYaw);
    const float cosYaw = std::cos(gameState_.playerYaw);
    const Vec3 forward {sinYaw, 0.0f, -cosYaw};
    const Vec3 right {cosYaw, 0.0f, sinYaw};

    if (keys[SDL_SCANCODE_W] != 0) {
        move = move + forward;
    }
    if (keys[SDL_SCANCODE_S] != 0) {
        move = move - forward;
    }
    if (keys[SDL_SCANCODE_A] != 0) {
        move = move - right;
    }
    if (keys[SDL_SCANCODE_D] != 0) {
        move = move + right;
    }

    if (keys[SDL_SCANCODE_LEFT] != 0) {
        gameState_.playerYaw -= 1.8f * deltaSeconds * control;
    }
    if (keys[SDL_SCANCODE_RIGHT] != 0) {
        gameState_.playerYaw += 1.8f * deltaSeconds * control;
    }

    const Bounds3 walkBounds = roomManager_.loaded()
        ? roomManager_.currentRoom().walkBounds
        : Bounds3 {{-1.85f, 0.0f, -1.85f}, {1.85f, 2.4f, 1.85f}};

    if (move.length() > 0.001f) {
        constexpr float kPlayerRadius = 0.28f;
        const bool running = (keys[SDL_SCANCODE_LSHIFT] != 0) || (keys[SDL_SCANCODE_RSHIFT] != 0);
        const float speed = (running ? (kWalkSpeed * kRunMultiplier) : kWalkSpeed) * control;
        const Vec3 delta = normalize(move) * (speed * deltaSeconds);
        const Vec3 startPosition = gameState_.playerPosition;

        Vec3 nextPosition = startPosition;
        Vec3 xCandidate = startPosition;
        xCandidate.x = std::clamp(startPosition.x + delta.x, walkBounds.min.x, walkBounds.max.x);
        if (!roomManager_.loaded() || !blockedByRoomCollider(xCandidate, roomManager_.currentRoom(), kPlayerRadius)) {
            nextPosition.x = xCandidate.x;
        }

        Vec3 zCandidate = nextPosition;
        zCandidate.z = std::clamp(startPosition.z + delta.z, walkBounds.min.z, walkBounds.max.z);
        if (!roomManager_.loaded() || !blockedByRoomCollider(zCandidate, roomManager_.currentRoom(), kPlayerRadius)) {
            nextPosition.z = zCandidate.z;
        }

        gameState_.playerPosition = nextPosition;
    }

    gameState_.playerPosition.x = std::clamp(gameState_.playerPosition.x, walkBounds.min.x, walkBounds.max.x);
    gameState_.playerPosition.z = std::clamp(gameState_.playerPosition.z, walkBounds.min.z, walkBounds.max.z);
    gameState_.playerPosition.y = kEyeHeight;

    const Vec3 frameMovement = gameState_.playerPosition - frameStartPosition;
    const float frameDistance = std::sqrt((frameMovement.x * frameMovement.x) + (frameMovement.z * frameMovement.z));
    const float movementSpeed = dt > 0.0001f ? frameDistance / dt : 0.0f;
    const float targetWalkAmount = std::clamp(movementSpeed / (kWalkSpeed * kRunMultiplier), 0.0f, 1.0f);
    const float walkResponse = 1.0f - std::exp(-dt * (targetWalkAmount > walkCameraAmount_ ? 9.0f : 6.0f));
    walkCameraAmount_ = lerpFloat(walkCameraAmount_, targetWalkAmount, walkResponse);
    if (walkCameraAmount_ > 0.001f) {
        walkCameraPhase_ += dt * (5.15f + (targetWalkAmount * 2.0f));
    }

    currentFocusPrompt_.clear();
    currentFocusInteractionId_.clear();
    if (roomManager_.loaded()) {
        if (const RoomDoor* door = roomManager_.doorAt(gameState_.playerPosition)) {
            currentFocusPrompt_ = door->prompt.empty() ? door->id : door->prompt;
        } else if (const RoomInteraction* interaction = roomManager_.interactionAt(gameState_.playerPosition)) {
            currentFocusPrompt_ = interaction->prompt.empty() ? interaction->id : interaction->prompt;
            currentFocusInteractionId_ = interaction->id;
        }
    }
}

void Application::updateLyingLimitedLook(float deltaSeconds, float mouseDeltaX, float mouseDeltaY) {
    constexpr float kMouseSensitivity = 0.00155f;
    constexpr float kKeyboardLookSpeed = 1.15f;
    constexpr float kMaxYaw = 90.0f * 0.017453292519943295769f;
    constexpr float kMinPitchOffset = -88.0f * 0.017453292519943295769f;
    constexpr float kMaxPitchOffset = 8.0f * 0.017453292519943295769f;
    constexpr float kMinAbsolutePitch = -12.0f * 0.017453292519943295769f;
    constexpr float kMaxAbsolutePitch = 86.0f * 0.017453292519943295769f;

    const float dt = std::max(deltaSeconds, 0.0f);
    lyingLookTimer_ += dt;

    if (lyingLookInputEnabled_) {
        lyingYawTarget_ += mouseDeltaX * kMouseSensitivity;
        lyingPitchTarget_ -= mouseDeltaY * kMouseSensitivity;

        if (const std::uint8_t* keys = SDL_GetKeyboardState(nullptr)) {
            if (keys[SDL_SCANCODE_LEFT] != 0) {
                lyingYawTarget_ -= kKeyboardLookSpeed * dt;
            }
            if (keys[SDL_SCANCODE_RIGHT] != 0) {
                lyingYawTarget_ += kKeyboardLookSpeed * dt;
            }
            if (keys[SDL_SCANCODE_UP] != 0) {
                lyingPitchTarget_ += kKeyboardLookSpeed * dt;
            }
            if (keys[SDL_SCANCODE_DOWN] != 0) {
                lyingPitchTarget_ -= kKeyboardLookSpeed * dt;
            }
        }
    } else {
        const float settle = 1.0f - std::exp(-dt * 3.0f);
        lyingYawTarget_ = lerpFloat(lyingYawTarget_, 0.0f, settle);
        lyingPitchTarget_ = lerpFloat(lyingPitchTarget_, 0.0f, settle);
    }

    lyingYawTarget_ = std::clamp(lyingYawTarget_, -kMaxYaw, kMaxYaw);
    lyingPitchTarget_ = std::clamp(lyingPitchTarget_, kMinPitchOffset, kMaxPitchOffset);

    const float inertia = 1.0f - std::exp(-dt * 6.5f);
    lyingYawOffset_ = lerpFloat(lyingYawOffset_, lyingYawTarget_, inertia);
    lyingPitchOffset_ = lerpFloat(lyingPitchOffset_, lyingPitchTarget_, inertia);

    gameState_.playerPosition = lyingAnchorPosition_;
    gameState_.playerYaw = lyingBaseYaw_ + lyingYawOffset_;
    playerPitch_ = std::clamp(lyingBasePitch_ + lyingPitchOffset_, kMinAbsolutePitch, kMaxAbsolutePitch);
    currentFocusPrompt_.clear();
    currentFocusInteractionId_.clear();
}

bool Application::loadStartupScene(bool required) {
    if (loadRoomScene(gameState_.roomId, gameState_.spawnId, false)) {
        return true;
    }

    if (!roomManager_.lastError().empty()) {
        Logger::warn(roomManager_.lastError());
    }

    const std::filesystem::path scenePath = engineRoot() / "samples" / "reference_scene.json";

    try {
        scene_ = SceneLoader::loadFromFile(scenePath);
        Logger::info("Scene JSON loaded: " + scenePath.string());
        return true;
    } catch (const std::exception& error) {
        Logger::error(error.what());
        if (required) {
            return false;
        }
    }

    scene_ = Scene::createReferenceScene();
    return false;
}

bool Application::loadRoomScene(const std::string& roomId, const std::string& spawnId, bool required) {
    if (!roomManager_.loadRoom(dataRoot(), roomId, spawnId)) {
        if (required) {
            Logger::error(roomManager_.lastError());
        }
        return false;
    }

    gameState_.roomId = roomManager_.currentRoom().id;
    gameState_.spawnId = roomManager_.activeSpawn().id;
    gameState_.playerPosition = roomManager_.activeSpawn().position;
    gameState_.playerYaw = roomManager_.activeSpawn().yaw;

    std::filesystem::path scenePath = roomManager_.currentRoom().scenePath;
    if (scenePath.empty()) {
        scenePath = roomManager_.currentRoom().sourcePath;
    }
    if (!scenePath.is_absolute()) {
        scenePath = engineRoot() / scenePath;
    }

    try {
        Logger::info("[RoomLoad] loading " + scenePath.string());
        scene_ = SceneLoader::loadFromFile(scenePath);
        Logger::info("[RoomLoad] scene=" + scene_.name()
            + " room=" + roomManager_.currentRoom().id
            + " staticMeshes=" + std::to_string(scene_.staticMeshes().size())
            + " pointLights=" + std::to_string(scene_.pointLights().size())
            + " fixedCameras=" + std::to_string(scene_.cameraRig().shots().size()));
        if (scene_.staticMeshes().empty()) {
            Logger::error("[RoomLoad] staticMeshes is empty: " + scenePath.string());
        }
        const std::filesystem::path root = engineRoot();
        for (const StaticMeshInstance& mesh : scene_.staticMeshes()) {
            std::filesystem::path source(mesh.meshSource);
            if (!source.empty() && !source.is_absolute()) {
                source = root / source;
            }
            const bool exists = !source.empty() && std::filesystem::exists(source);
            Logger::info("[MeshResolve] " + mesh.name
                + " meshSource=" + (mesh.meshSource.empty() ? std::string("<empty>") : mesh.meshSource)
                + " exists=" + (exists ? std::string("true") : std::string("false")));
        }
        Logger::info("Room loaded: " + roomManager_.currentRoom().id + " -> " + scenePath.string());
        sequenceHiddenEntities_.clear();
        animationSystem_.clear();
        characterPerformances_.clear();
        if (renderer_.stats().frameIndex > 0 || !sceneMeshHandles_.empty()) {
            reloadSceneMeshes();
        }
        cameraMode_ = CameraMode::FreeFirstPerson;
        lyingAnchorPosition_ = gameState_.playerPosition;
        lyingBaseYaw_ = gameState_.playerYaw;
        lyingBasePitch_ = 0.0f;
        lyingYawOffset_ = 0.0f;
        lyingPitchOffset_ = 0.0f;
        lyingYawTarget_ = 0.0f;
        lyingPitchTarget_ = 0.0f;
        lyingLookTimer_ = 0.0f;
        lyingLookInputEnabled_ = false;
        sequencePlayerControlLocked_ = false;
        sequenceOverlay_ = {};
        sequenceFadeStart_ = 0.0f;
        sequenceFadeTarget_ = 0.0f;
        sequenceFadeDuration_ = 0.0f;
        sequenceFadeTimer_ = 0.0f;
        sequenceManager_.setSequences(roomManager_.currentRoom().sequences, gameState_.flags);
        if (audioSystem_.available()) {
            processRoomEnterEvents();
        }
        return true;
    } catch (const std::exception& error) {
        Logger::error(error.what());
        return false;
    }
}

bool Application::loadStartupStory(bool required) {
    std::filesystem::path storyPath = engineRoot() / "data" / "story" / "zero_patient.story.json";
    if (!std::filesystem::exists(storyPath)) {
        storyPath = engineRoot() / "samples" / "zero_patient_story.json";
    }

    if (story_.loadFromFile(storyPath)) {
        Logger::info("Story JSON loaded: " + storyPath.string());
        syncStoryToGameState();
        return true;
    }

    Logger::error(story_.lastError());
    return !required;
}

void Application::reloadSceneMeshes() {
    sceneMeshHandles_.assign(scene_.staticMeshes().size(), -1);
    const std::filesystem::path root = engineRoot();
    const std::filesystem::path debugMesh = root / "assets" / "debug" / "debug_missing_mesh.obj";
    for (std::size_t i = 0; i < scene_.staticMeshes().size(); ++i) {
        const StaticMeshInstance& instance = scene_.staticMeshes()[i];
        std::filesystem::path meshAssetPath(instance.meshAsset);
        if (!meshAssetPath.empty() && !meshAssetPath.is_absolute()) {
            meshAssetPath = root / meshAssetPath;
        }

        std::filesystem::path source(instance.meshSource);
        if (!source.empty() && !source.is_absolute()) {
            source = root / source;
        }

        if (!instance.meshAsset.empty() && !std::filesystem::exists(meshAssetPath)) {
            Logger::warn("[MeshResolve] compiled meshAsset missing for " + instance.name
                + ": " + instance.meshAsset + "; using meshSource");
        }

        std::filesystem::path loadPath;
        if (!source.empty() && std::filesystem::exists(source)) {
            loadPath = source;
        } else {
            Logger::error("[MeshResolve] missing meshSource for static mesh instance: " + instance.name
                + " source=" + (instance.meshSource.empty() ? std::string("<empty>") : instance.meshSource));
            loadPath = debugMesh;
        }

        std::int32_t handle = renderer_.loadSceneMesh(loadPath);
        if (handle < 0 && loadPath != debugMesh) {
            Logger::error("[MeshResolve] mesh load failed for " + instance.name
                + "; using debug placeholder: " + debugMesh.string());
            handle = renderer_.loadSceneMesh(debugMesh);
        }
        sceneMeshHandles_[i] = handle;

        const std::string ext = lowerExtension(loadPath);
        if (instance.animationRig && (ext == ".glb" || ext == ".gltf")) {
            try {
                GltfModelData rigData = GltfLoader::loadFromFile(loadPath);
                animationSystem_.registerRig(instance.name, std::move(rigData), instance.defaultClip);
            } catch (const std::exception& error) {
                Logger::warn("Animation rig load failed for " + instance.name + ": " + error.what());
            }
        }
    }
}

void Application::activateCurrentFocus() {
    if (!roomManager_.loaded()) {
        return;
    }

    if (const RoomDoor* door = roomManager_.doorAt(gameState_.playerPosition)) {
        if (door->locked && !InventorySystem::hasItem(gameState_, door->requiredItem)) {
            Logger::warn("Door locked: " + door->id);
            return;
        }

        const std::string doorId = door->id;
        const std::string targetRoom = door->targetRoom;
        const std::string targetSpawn = door->targetSpawn;
        const std::string storyNode = door->storyNode;

        if (!storyNode.empty() && story_.jumpTo(storyNode)) {
            syncStoryToGameState();
        }

        if (loadRoomScene(targetRoom, targetSpawn, false)) {
            Logger::info("Door transition: " + doorId + " -> " + targetRoom + "." + targetSpawn);
        } else {
            Logger::warn("Door transition failed: " + doorId + " -> " + targetRoom);
        }
        return;
    }

    if (const RoomInteraction* interaction = roomManager_.interactionAt(gameState_.playerPosition)) {
        InteractionResult result = InteractionSystem::activate(gameState_, *interaction);
        if (!result.activated) {
            Logger::warn(result.message);
            return;
        }

        if (!result.storyNode.empty() && story_.jumpTo(result.storyNode)) {
            story_.restoreState(story_.currentNodeId(), gameState_.identity);
            syncStoryToGameState();
        }
        if (interaction->id == "anton_desk" && interaction->uiOverlay == "workstation") {
            beginWorkstationEntrySequence();
        } else if (!interaction->uiOverlay.empty()) {
            pendingUiOverlay_ = interaction->uiOverlay;
        }
        Logger::info("Interaction: " + interaction->id);
    }
}

void Application::beginWorkstationEntrySequence() {
    if (workstationSequenceState_ == WorkstationSequenceState::Entering
        || workstationSequenceState_ == WorkstationSequenceState::AtWorkstation
        || workstationSequenceState_ == WorkstationSequenceState::Exiting) {
        return;
    }

    workstationStandPosition_ = gameState_.playerPosition;
    workstationStandYaw_ = gameState_.playerYaw;
    workstationStandPitch_ = playerPitch_;
    workstationSequenceTimer_ = 0.0f;
    workstationSequenceState_ = WorkstationSequenceState::Entering;
    SDL_SetRelativeMouseMode(SDL_FALSE);
    Logger::info("Workstation enter sequence started");
}

void Application::beginWorkstationExitSequence() {
    workstationSequenceTimer_ = 0.0f;
    workstationSequenceState_ = WorkstationSequenceState::Exiting;
    gameState_.playerPosition = workstationSeatedPosition_;
    gameState_.playerYaw = workstationSeatedYaw_;
    playerPitch_ = workstationSeatedPitch_;
    SDL_SetRelativeMouseMode(SDL_FALSE);
    Logger::info("Workstation exit sequence started");
}

bool Application::updateWorkstationSequence(float deltaSeconds) {
    constexpr float kEnterDuration = 1.25f;
    constexpr float kExitDuration = 1.05f;

    if (workstationSequenceState_ == WorkstationSequenceState::Entering) {
        workstationSequenceTimer_ += std::max(deltaSeconds, 0.0f);
        const float t = smoothStep01(workstationSequenceTimer_ / kEnterDuration);
        gameState_.playerPosition = lerpVec3(workstationStandPosition_, workstationSeatedPosition_, t);
        gameState_.playerYaw = lerpFloat(workstationStandYaw_, workstationSeatedYaw_, t);
        playerPitch_ = lerpFloat(workstationStandPitch_, workstationSeatedPitch_, t);

        if (workstationSequenceTimer_ >= kEnterDuration) {
            gameState_.playerPosition = workstationSeatedPosition_;
            gameState_.playerYaw = workstationSeatedYaw_;
            playerPitch_ = workstationSeatedPitch_;
            workstationSequenceState_ = WorkstationSequenceState::AtWorkstation;
            workstationSequenceTimer_ = 0.0f;
            return true;
        }
    } else if (workstationSequenceState_ == WorkstationSequenceState::Exiting) {
        workstationSequenceTimer_ += std::max(deltaSeconds, 0.0f);
        const float t = smoothStep01(workstationSequenceTimer_ / kExitDuration);
        gameState_.playerPosition = lerpVec3(workstationSeatedPosition_, workstationStandPosition_, t);
        gameState_.playerYaw = lerpFloat(workstationSeatedYaw_, workstationStandYaw_, t);
        playerPitch_ = lerpFloat(workstationSeatedPitch_, workstationStandPitch_, t);

        if (workstationSequenceTimer_ >= kExitDuration) {
            gameState_.playerPosition = workstationStandPosition_;
            gameState_.playerYaw = workstationStandYaw_;
            playerPitch_ = workstationStandPitch_;
            workstationSequenceState_ = WorkstationSequenceState::None;
            workstationSequenceTimer_ = 0.0f;
            if (config_.maxFrames == 0) {
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            armCollapseAfterWorkstation();
            Logger::info("Workstation exit sequence completed");
        }
    }

    return false;
}

bool Application::workstationSequenceBlocksPlayer() const {
    return workstationSequenceState_ == WorkstationSequenceState::Entering
        || workstationSequenceState_ == WorkstationSequenceState::AtWorkstation
        || workstationSequenceState_ == WorkstationSequenceState::Exiting;
}

void Application::updateSequenceRuntime(float deltaSeconds) {
    animationSystem_.update(deltaSeconds);
    updateCharacterPerformances(deltaSeconds);

    if (sequenceFadeDuration_ > 0.0f && sequenceFadeTimer_ < sequenceFadeDuration_) {
        sequenceFadeTimer_ = std::min(sequenceFadeTimer_ + std::max(deltaSeconds, 0.0f), sequenceFadeDuration_);
        const float t = smoothStep01(sequenceFadeTimer_ / sequenceFadeDuration_);
        sequenceOverlay_.blackFade = lerpFloat(sequenceFadeStart_, sequenceFadeTarget_, t);
    }

    for (const SequenceAction& action : sequenceManager_.update(deltaSeconds)) {
        executeSequenceAction(action);
    }
}

void Application::executeSequenceAction(const SequenceAction& action) {
    if (action.type == "setFlag") {
        if (action.flag.empty()) {
            Logger::warn("Sequence setFlag action missing flag");
            return;
        }
        gameState_.flags.insert(action.flag);
        sequenceManager_.startAutoSequences(gameState_.flags);
        return;
    }

    if (action.type == "playAudio") {
        playAudioCue(action.audioCue, action.loop);
        return;
    }

    if (action.type == "stopAudio") {
        audioSystem_.stopMusic();
        return;
    }

    if (action.type == "setMusicVolume") {
        if (action.volume < 0.0f) {
            Logger::warn("Sequence setMusicVolume action missing volume");
            return;
        }
        audioSystem_.setMusicVolume(std::clamp(action.volume, 0.0f, 1.0f));
        return;
    }

    if (action.type == "fadeScreen") {
        sequenceFadeStart_ = sequenceOverlay_.blackFade;
        sequenceFadeTarget_ = std::clamp(action.blackFade, 0.0f, 1.0f);
        sequenceFadeDuration_ = std::max(action.duration, 0.0f);
        sequenceFadeTimer_ = 0.0f;
        sequenceOverlay_.noiseIntensity = std::clamp(action.noiseIntensity, 0.0f, 1.0f);
        if (sequenceFadeDuration_ <= 0.0f) {
            sequenceOverlay_.blackFade = sequenceFadeTarget_;
        }
        return;
    }

    if (action.type == "setCameraMode") {
        if (action.cameraMode == "collapse_dizzy"
            || action.cameraMode == "collapse_sway"
            || action.cameraMode == "collapse_camera_sway") {
            setCollapseCameraStage(CollapseSequenceState::Dizzy);
            return;
        }
        if (action.cameraMode == "collapse_falling"
            || action.cameraMode == "collapse_fall"
            || action.cameraMode == "collapse_camera_fall") {
            setCollapseCameraStage(CollapseSequenceState::Falling);
            return;
        }
        if (action.cameraMode == "collapse_blackout") {
            setCollapseCameraStage(CollapseSequenceState::Blackout);
            return;
        }
        if (action.cameraMode == "LyingLimitedLook"
            || action.cameraMode == "lying_limited_look"
            || action.cameraMode == "lying"
            || action.cameraMode == "limited") {
            const std::string anchorId = action.entityId.empty() ? std::string("stretcher_head_anchor") : action.entityId;
            const bool inputEnabled = action.cameraMode == "limited";
            enterLyingLimitedLook(anchorId, inputEnabled);
            return;
        }
        if (action.cameraMode == "SeatedComputer" || action.cameraMode == "seated_computer") {
            cameraMode_ = CameraMode::SeatedComputer;
            return;
        }
        if (action.cameraMode == "CollapseCutscene" || action.cameraMode == "collapse_cutscene") {
            cameraMode_ = CameraMode::CollapseCutscene;
            return;
        }
        if (action.cameraMode == "player"
            || action.cameraMode == "default"
            || action.cameraMode == "none") {
            resetCollapseRuntime();
            cameraMode_ = CameraMode::FreeFirstPerson;
            return;
        }
        Logger::warn("Sequence setCameraMode is not implemented yet: " + action.cameraMode);
        return;
    }

    if (action.type == "lockPlayerControl") {
        sequencePlayerControlLocked_ = true;
        if (action.cameraMode == "lying"
            || action.cameraMode == "limited"
            || action.cameraMode == "LyingLimitedLook"
            || action.cameraMode == "lying_limited_look") {
            const std::string anchorId = action.entityId.empty() ? std::string("stretcher_head_anchor") : action.entityId;
            const bool inputEnabled = action.cameraMode == "limited";
            enterLyingLimitedLook(anchorId, inputEnabled);
        }
        return;
    }

    if (action.type == "unlockPlayerControl") {
        sequencePlayerControlLocked_ = false;
        if (lyingLimitedLookActive()) {
            cameraMode_ = CameraMode::FreeFirstPerson;
        }
        return;
    }

    if (action.type == "transitionRoom") {
        if (action.roomId.empty()) {
            Logger::warn("Sequence transitionRoom action missing roomId");
            return;
        }
        const std::string spawnId = action.spawnId.empty() ? "entry" : action.spawnId;
        if (!loadRoomScene(action.roomId, spawnId, false)) {
            Logger::warn("Sequence transitionRoom failed: " + action.roomId + "." + spawnId);
        } else {
            resetCollapseRuntime();
            updateSequenceRuntime(0.0f);
        }
        return;
    }

    if (action.type == "setEntityVisible") {
        if (action.entityId.empty()) {
            Logger::warn("Sequence setEntityVisible action missing entityId");
            return;
        }
        if (action.visible) {
            sequenceHiddenEntities_.erase(action.entityId);
        } else {
            sequenceHiddenEntities_.insert(action.entityId);
        }
        return;
    }

    if (action.type == "setEntityTransform") {
        setEntityTransformOverride(action);
        return;
    }

    if (action.type == "animateEntityTransform") {
        animateEntityTransformOverride(action);
        return;
    }

    if (action.type == "clearEntityTransform") {
        clearEntityTransformOverride(action);
        return;
    }

    if (action.type == "playAnimation") {
        if (action.entityId.empty() || action.clipId.empty()) {
            Logger::warn("Sequence playAnimation action missing entityId or clipId");
            return;
        }
        animationSystem_.setPlaybackSpeed(action.entityId, action.playbackSpeed);
        animationSystem_.playClip(action.entityId, action.clipId, action.loop, action.fadeSeconds);
        return;
    }

    if (action.type == "stopAnimation") {
        if (action.entityId.empty()) {
            Logger::warn("Sequence stopAnimation action missing entityId");
            return;
        }
        animationSystem_.stopClip(action.entityId, action.fadeSeconds);
        return;
    }

    if (action.type == "setFacialCue") {
        if (action.entityId.empty()) {
            Logger::warn("Sequence setFacialCue action missing entityId");
            return;
        }
        const std::string cue = !action.cueId.empty() ? action.cueId : action.audioCue;
        animationSystem_.setFacialCue(action.entityId, cue, action.intensity, action.duration);
        return;
    }

    if (action.type == "setLookAtTarget") {
        Logger::warn("Sequence setLookAtTarget is not implemented yet; action skipped safely");
        return;
    }

    if (action.type == "startCharacterPerformance" || action.type == "playCharacterPerformance") {
        startCharacterPerformance(action);
        return;
    }

    if (action.type == "stopCharacterPerformance") {
        stopCharacterPerformance(action);
        return;
    }

    Logger::warn("Unknown sequence action type: " + action.type);
}

bool Application::sequenceBlocksPlayer() const {
    return sequencePlayerControlLocked_;
}

ScreenOverlay Application::currentScreenOverlay() const {
    const ScreenOverlay collapse = collapseScreenOverlay();
    return {
        .blackFade = std::max(collapse.blackFade, sequenceOverlay_.blackFade),
        .noiseIntensity = std::max(collapse.noiseIntensity, sequenceOverlay_.noiseIntensity),
    };
}

void Application::armCollapseAfterWorkstation() {
    if (!roomManager_.loaded() || collapseSequenceState_ != CollapseSequenceState::None) {
        return;
    }

    const RoomDefinition& room = roomManager_.currentRoom();
    if (room.collapseAfterFlag.empty()
        || !gameState_.flags.contains(room.collapseAfterFlag)
        || gameState_.flags.contains("collapse_completed")
        || gameState_.flags.contains("flag_anton_collapse_complete")) {
        return;
    }

    collapseArmPosition_ = gameState_.playerPosition;
    collapseSequenceTimer_ = 0.0f;
    collapseControlMultiplier_ = 1.0f;
    collapseBlackFade_ = 0.0f;
    collapseNoiseIntensity_ = 0.0f;
    collapseCameraRoll_ = 0.0f;
    collapseStoredMusicVolume_ = audioSystem_.musicVolume();

    if (!room.collapseSequenceId.empty() && sequenceManager_.startSequence(room.collapseSequenceId, gameState_.flags)) {
        collapseDrivenBySequence_ = true;
        collapseSequenceState_ = CollapseSequenceState::None;
        sequencePlayerControlLocked_ = true;
        Logger::info("Collapse timeline sequence started after workstation: " + room.collapseSequenceId);
        return;
    }

    Logger::warn("Collapse timeline unavailable after workstation; using legacy collapse runtime");
    beginCollapseDizzy();
}

void Application::skipToCollapseShortcut() {
    if (gameState_.roomId == "ambulance_patient_compartment") {
        gameState_.flags.insert("entered_ambulance");
        sequenceManager_.startAutoSequences(gameState_.flags);
        updateSequenceRuntime(0.0f);
        Logger::info("Skip-to-collapse ignored: already in ambulance");
        return;
    }

    if (!roomManager_.loaded() || roomManager_.currentRoom().collapseTargetRoom.empty()) {
        if (!loadRoomScene("office_open_space_3f", "from_elevator_day", false)) {
            Logger::warn("Skip-to-collapse failed: office room is unavailable");
            return;
        }
        playRoomMusic();
    }

    if (!roomManager_.loaded()) {
        return;
    }

    const RoomDefinition& room = roomManager_.currentRoom();
    if (room.collapseTargetRoom.empty()) {
        Logger::warn("Skip-to-collapse failed: current room has no collapse target");
        return;
    }
    const std::string targetRoom = room.collapseTargetRoom;
    const std::string targetSpawn = room.collapseTargetSpawn.empty()
        ? std::string("stretcher_head_spawn")
        : room.collapseTargetSpawn;
    const std::string enteredFlag = room.collapseTargetEnteredFlag.empty()
        ? std::string("entered_ambulance")
        : room.collapseTargetEnteredFlag;

    workstationSequenceState_ = WorkstationSequenceState::None;
    workstationSequenceTimer_ = 0.0f;
    pendingUiOverlay_.clear();
    currentFocusPrompt_.clear();
    currentFocusInteractionId_.clear();
    sequencePlayerControlLocked_ = false;
    sequenceOverlay_ = {};
    sequenceHiddenEntities_.clear();
    animationSystem_.clear();
    characterPerformances_.clear();

    gameState_.flags.insert("flag_workstation_task_complete");
    gameState_.flags.insert("office_report_completed");
    gameState_.flags.insert("office_pc_shutdown");

    if (gameState_.flags.contains("collapse_completed") || gameState_.flags.contains("flag_anton_collapse_complete")) {
        gameState_.flags.insert(enteredFlag);
        if (loadRoomScene(targetRoom, targetSpawn, false)) {
            updateSequenceRuntime(0.0f);
            Logger::info("Skip-to-collapse loaded already completed target: " + targetRoom);
        }
        return;
    }

    if (sequenceManager_.running() || collapseSequenceState_ != CollapseSequenceState::None) {
        completeCollapseTransition();
        Logger::info("Skip-to-collapse fast-forwarded active collapse");
        return;
    }

    collapseArmPosition_ = gameState_.playerPosition;
    collapseSequenceTimer_ = 0.0f;
    collapseControlMultiplier_ = 1.0f;
    collapseBlackFade_ = 0.0f;
    collapseNoiseIntensity_ = 0.0f;
    collapseCameraRoll_ = 0.0f;
    collapseStoredMusicVolume_ = audioSystem_.musicVolume();

    if (!room.collapseSequenceId.empty() && sequenceManager_.startSequence(room.collapseSequenceId, gameState_.flags)) {
        collapseDrivenBySequence_ = true;
        collapseSequenceState_ = CollapseSequenceState::None;
        Logger::info("Skip-to-collapse started timeline: " + room.collapseSequenceId);
        return;
    }

    Logger::warn("Skip-to-collapse falling back to legacy collapse runtime");
    beginCollapseDizzy();
}

void Application::beginCollapseDizzy() {
    if (!roomManager_.loaded()) {
        return;
    }

    collapseDrivenBySequence_ = false;
    collapseStartPosition_ = gameState_.playerPosition;
    collapseStartYaw_ = gameState_.playerYaw;
    collapseStartPitch_ = playerPitch_;
    collapseSequenceTimer_ = 0.0f;
    collapseStoredMusicVolume_ = audioSystem_.musicVolume();
    audioSystem_.setMusicVolume(std::min(collapseStoredMusicVolume_, 0.22f));
    gameState_.flags.insert("collapse_started");
    gameState_.flags.insert("flag_anton_collapse_started");
    playAudioCue(roomManager_.currentRoom().collapseAudioCue);
    collapseSequenceState_ = CollapseSequenceState::Dizzy;
    Logger::info("Collapse sequence started");
}

void Application::setCollapseCameraStage(CollapseSequenceState stage) {
    if (stage == CollapseSequenceState::None || stage == CollapseSequenceState::ArmedAfterWorkstation) {
        resetCollapseRuntime();
        return;
    }

    cameraMode_ = CameraMode::CollapseCutscene;
    collapseDrivenBySequence_ = true;
    collapseSequenceState_ = stage;
    collapseSequenceTimer_ = 0.0f;

    if (stage == CollapseSequenceState::Dizzy || stage == CollapseSequenceState::Falling) {
        collapseStartPosition_ = gameState_.playerPosition;
        collapseStartYaw_ = gameState_.playerYaw;
        collapseStartPitch_ = playerPitch_;
    }

    if (stage == CollapseSequenceState::Dizzy) {
        collapseStoredMusicVolume_ = audioSystem_.musicVolume();
        collapseControlMultiplier_ = 0.78f;
        collapseBlackFade_ = 0.10f;
        collapseNoiseIntensity_ = 0.08f;
        collapseCameraRoll_ = 0.0f;
        audioSystem_.setMusicVolume(std::min(collapseStoredMusicVolume_, 0.22f));
        return;
    }

    if (stage == CollapseSequenceState::Falling) {
        collapseControlMultiplier_ = 0.0f;
        collapseBlackFade_ = std::max(collapseBlackFade_, 0.62f);
        collapseNoiseIntensity_ = std::max(collapseNoiseIntensity_, 0.38f);
        collapseCameraRoll_ = std::max(collapseCameraRoll_, 0.25f);
        audioSystem_.setMusicVolume(std::min(collapseStoredMusicVolume_, 0.08f));
        SDL_SetRelativeMouseMode(SDL_FALSE);
        return;
    }

    collapseControlMultiplier_ = 0.0f;
    collapseBlackFade_ = 1.0f;
    collapseNoiseIntensity_ = std::max(collapseNoiseIntensity_, 0.58f);
    collapseCameraRoll_ = 1.18f;
    SDL_SetRelativeMouseMode(SDL_FALSE);
}

void Application::resetCollapseRuntime() {
    collapseSequenceState_ = CollapseSequenceState::None;
    collapseSequenceTimer_ = 0.0f;
    collapseControlMultiplier_ = 1.0f;
    collapseBlackFade_ = 0.0f;
    collapseNoiseIntensity_ = 0.0f;
    collapseCameraRoll_ = 0.0f;
    collapseDrivenBySequence_ = false;
    if (cameraMode_ == CameraMode::CollapseCutscene) {
        cameraMode_ = CameraMode::FreeFirstPerson;
    }
}

void Application::enterLyingLimitedLook(const std::string& anchorId, bool inputEnabled) {
    constexpr float kCeilingLookPitch = 76.0f * 0.017453292519943295769f;

    cameraMode_ = CameraMode::LyingLimitedLook;
    lyingAnchorPosition_ = roomAnchorPosition(anchorId, gameState_.playerPosition);
    lyingBaseYaw_ = gameState_.playerYaw;
    lyingBasePitch_ = kCeilingLookPitch;
    lyingYawOffset_ = 0.0f;
    lyingPitchOffset_ = 0.0f;
    lyingYawTarget_ = 0.0f;
    lyingPitchTarget_ = 0.0f;
    lyingLookTimer_ = 0.0f;
    lyingLookInputEnabled_ = inputEnabled;
    gameState_.playerPosition = lyingAnchorPosition_;
    playerPitch_ = lyingBasePitch_;
    if (config_.maxFrames == 0) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
    Logger::info("Camera mode: LyingLimitedLook at " + anchorId);
}

bool Application::lyingLimitedLookActive() const {
    return cameraMode_ == CameraMode::LyingLimitedLook;
}

Vec3 Application::roomAnchorPosition(const std::string& anchorId, Vec3 fallback) const {
    if (!roomManager_.loaded() || anchorId.empty()) {
        return fallback;
    }

    const RoomDefinition& room = roomManager_.currentRoom();
    for (const RoomSpawn& spawn : room.spawns) {
        if (spawn.id == anchorId) {
            return spawn.position;
        }
    }
    for (const RoomInteraction& interaction : room.interactions) {
        if (interaction.id == anchorId) {
            return {
                (interaction.bounds.min.x + interaction.bounds.max.x) * 0.5f,
                (interaction.bounds.min.y + interaction.bounds.max.y) * 0.5f,
                (interaction.bounds.min.z + interaction.bounds.max.z) * 0.5f,
            };
        }
    }

    Logger::warn("Camera anchor not found: " + anchorId);
    return fallback;
}

void Application::updateCollapseSequence(float deltaSeconds) {
    constexpr float kWalkDistanceToCollapse = 0.95f;
    constexpr float kForcedStartSeconds = 2.2f;
    constexpr float kDizzyDuration = 3.15f;
    constexpr float kFallDuration = 1.30f;
    constexpr float kBlackoutDuration = 1.05f;

    const float dt = std::max(deltaSeconds, 0.0f);

    if (collapseSequenceState_ == CollapseSequenceState::None) {
        collapseControlMultiplier_ = 1.0f;
        collapseBlackFade_ = 0.0f;
        collapseNoiseIntensity_ = 0.0f;
        collapseCameraRoll_ = 0.0f;
        return;
    }

    if (collapseSequenceState_ == CollapseSequenceState::ArmedAfterWorkstation) {
        collapseSequenceTimer_ += dt;
        collapseControlMultiplier_ = 1.0f;
        collapseBlackFade_ = 0.0f;
        collapseNoiseIntensity_ = 0.0f;
        collapseCameraRoll_ = 0.0f;

        if (distanceXz(gameState_.playerPosition, collapseArmPosition_) >= kWalkDistanceToCollapse
            || collapseSequenceTimer_ >= kForcedStartSeconds) {
            const RoomDefinition& room = roomManager_.currentRoom();
            if (!room.collapseSequenceId.empty()) {
                if (sequenceManager_.startSequence(room.collapseSequenceId, gameState_.flags)) {
                    collapseDrivenBySequence_ = true;
                    collapseSequenceState_ = CollapseSequenceState::None;
                    collapseSequenceTimer_ = 0.0f;
                    Logger::info("Collapse timeline sequence started: " + room.collapseSequenceId);
                    return;
                }
                Logger::warn("Collapse timeline sequence did not start: " + room.collapseSequenceId);
            }
            beginCollapseDizzy();
        }
        return;
    }

    if (collapseSequenceState_ == CollapseSequenceState::Dizzy) {
        collapseSequenceTimer_ += dt;
        const float t = smoothStep01(collapseSequenceTimer_ / kDizzyDuration);
        const float pulse = (std::sin(collapseSequenceTimer_ * 9.0f) + 1.0f) * 0.5f;
        collapseControlMultiplier_ = lerpFloat(0.78f, 0.05f, t);
        collapseBlackFade_ = std::clamp(0.10f + (0.55f * t) + (pulse * 0.05f * t), 0.0f, 0.78f);
        collapseNoiseIntensity_ = std::clamp(0.08f + (0.33f * t) + (pulse * 0.08f), 0.0f, 0.55f);
        collapseCameraRoll_ = (std::sin(collapseSequenceTimer_ * 4.6f) * 0.035f) + (t * 0.28f);

        if (!collapseDrivenBySequence_ && collapseSequenceTimer_ >= kDizzyDuration) {
            collapseSequenceState_ = CollapseSequenceState::Falling;
            collapseSequenceTimer_ = 0.0f;
            collapseStartPosition_ = gameState_.playerPosition;
            collapseStartYaw_ = gameState_.playerYaw;
            collapseStartPitch_ = playerPitch_;
            gameState_.flags.insert("flag_anton_collapse_falling");
            audioSystem_.setMusicVolume(std::min(collapseStoredMusicVolume_, 0.08f));
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }
        return;
    }

    if (collapseSequenceState_ == CollapseSequenceState::Falling) {
        collapseSequenceTimer_ += dt;
        const float t = smoothStep01(collapseSequenceTimer_ / kFallDuration);
        collapseControlMultiplier_ = 0.0f;
        collapseBlackFade_ = std::clamp(lerpFloat(0.62f, 0.97f, t), 0.0f, 1.0f);
        collapseNoiseIntensity_ = std::clamp(lerpFloat(0.38f, 0.85f, t), 0.0f, 1.0f);
        collapseCameraRoll_ = lerpFloat(0.25f, 1.18f, t);

        if (!collapseDrivenBySequence_ && collapseSequenceTimer_ >= kFallDuration) {
            collapseSequenceState_ = CollapseSequenceState::Blackout;
            collapseSequenceTimer_ = 0.0f;
            gameState_.flags.insert("flag_anton_collapse_blackout");
            if (roomManager_.loaded()) {
                playAudioCue(roomManager_.currentRoom().collapseHeartbeatAudioCue);
            }
        }
        return;
    }

    if (collapseSequenceState_ == CollapseSequenceState::Blackout) {
        collapseSequenceTimer_ += dt;
        const float t = smoothStep01(collapseSequenceTimer_ / kBlackoutDuration);
        collapseControlMultiplier_ = 0.0f;
        collapseBlackFade_ = 1.0f;
        collapseNoiseIntensity_ = lerpFloat(0.58f, 0.04f, t);
        collapseCameraRoll_ = 1.18f;

        if (!collapseDrivenBySequence_
            && collapseSequenceTimer_ >= kBlackoutDuration
            && !gameState_.flags.contains("collapse_completed")) {
            completeCollapseTransition();
        }
        return;
    }
}

void Application::completeCollapseTransition() {
    if (!roomManager_.loaded()) {
        return;
    }

    // TODO: move this whole collapse chain to a data-driven sequence asset once the next scene is authored.
    const RoomDefinition sourceRoom = roomManager_.currentRoom();
    gameState_.flags.insert("collapse_completed");
    gameState_.flags.insert("flag_anton_collapse_complete");
    audioSystem_.stopMusic();

    if (sourceRoom.collapseTargetRoom.empty()) {
        Logger::warn("Collapse completed without a target room");
        return;
    }

    const std::string targetSpawn = sourceRoom.collapseTargetSpawn.empty()
        ? std::string("wake_on_stretcher")
        : sourceRoom.collapseTargetSpawn;
    bool insertedEnteredFlag = false;
    if (!sourceRoom.collapseTargetEnteredFlag.empty()) {
        gameState_.flags.insert(sourceRoom.collapseTargetEnteredFlag);
        insertedEnteredFlag = true;
    }
    if (!loadRoomScene(sourceRoom.collapseTargetRoom, targetSpawn, false)) {
        if (insertedEnteredFlag) {
            gameState_.flags.erase(sourceRoom.collapseTargetEnteredFlag);
        }
        Logger::warn("Collapse target room failed to load: " + sourceRoom.collapseTargetRoom);
        return;
    }

    resetCollapseRuntime();
    audioSystem_.setMusicVolume(collapseStoredMusicVolume_);
    updateSequenceRuntime(0.0f);
    if (config_.maxFrames == 0 && !sequenceBlocksPlayer()) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
    Logger::info("Collapse transition loaded target room: " + sourceRoom.collapseTargetRoom);
}

bool Application::collapseSequenceBlocksPlayer() const {
    return collapseSequenceState_ == CollapseSequenceState::Falling
        || collapseSequenceState_ == CollapseSequenceState::Blackout;
}

float Application::collapseControlMultiplier() const {
    return std::clamp(collapseControlMultiplier_, 0.0f, 1.0f);
}

ScreenOverlay Application::collapseScreenOverlay() const {
    return {
        .blackFade = collapseBlackFade_,
        .noiseIntensity = collapseNoiseIntensity_,
    };
}

Transform Application::animatedStaticMeshTransform(const StaticMeshInstance& instance) const {
    Transform result = animationSystem_.transformFor(instance.name, instance.transform);

    if (instance.name == "office_chair_pc_02") {
        float t = 0.0f;
        if (workstationSequenceState_ == WorkstationSequenceState::Entering) {
            t = smoothStep01(workstationSequenceTimer_ / 1.25f);
        } else if (workstationSequenceState_ == WorkstationSequenceState::AtWorkstation) {
            t = 1.0f;
        } else if (workstationSequenceState_ == WorkstationSequenceState::Exiting) {
            t = 1.0f - smoothStep01(workstationSequenceTimer_ / 1.05f);
        } else {
            return applyCharacterPerformance(instance.name, result);
        }

        const Vec3 seatedPosition {-1.22f, result.position.y, -0.78f};
        result.position = lerpVec3(result.position, seatedPosition, t);
        result.rotation.y = lerpFloat(result.rotation.y, 3.14159f, t);
    }

    return applyCharacterPerformance(instance.name, result);
}

Transform Application::applyCharacterPerformance(const std::string& entityId, Transform base) const {
    const auto activeIt = characterPerformances_.find(entityId);
    if (activeIt == characterPerformances_.end()) {
        return base;
    }

    const ActiveCharacterPerformance& performance = activeIt->second;
    const float duration = std::max(performance.duration, 0.001f);
    const float fadeIn = smoothStep01(std::min(performance.elapsed / 0.28f, 1.0f));
    const float fadeOut = smoothStep01(std::min((duration - performance.elapsed) / 0.45f, 1.0f));
    const float envelope = fadeIn * fadeOut * std::clamp(performance.intensity, 0.0f, 2.5f);
    if (envelope <= 0.0f) {
        return base;
    }

    const bool urgent = performance.cueId.find("losing") != std::string::npos
        || performance.cueId.find("urgent") != std::string::npos;
    const bool idle = performance.cueId.find("idle") != std::string::npos;
    const bool speech = performance.cueId.find("nurse_") != std::string::npos
        || performance.cueId.find("voice") != std::string::npos;
    const float time = performance.elapsed;
    const float breath = (std::sin(time * 1.45f) * 0.75f + std::sin(time * 2.35f + 0.45f) * 0.25f)
        * (idle ? 0.050f : 0.020f);
    const float bodySway = (std::sin(time * 0.72f + 0.7f) * 0.65f + std::sin(time * 1.18f + 1.4f) * 0.35f)
        * (idle ? 0.032f : 0.010f);
    const float weightShift = (std::sin(time * 0.54f + 1.1f) * 0.55f + std::sin(time * 0.96f) * 0.45f)
        * (idle ? 0.026f : 0.010f);
    const float speechPulse = speech ? std::abs(std::sin(time * (urgent ? 14.5f : 10.5f))) : 0.0f;
    const float stress = urgent ? 1.25f : 1.0f;

    base.position.y += (breath + (speechPulse * 0.007f)) * envelope;
    base.position.x += bodySway * envelope;
    base.position.z += weightShift * envelope;
    base.rotation.x += ((speech ? -0.038f : -0.025f) + (std::sin(time * 2.9f) * (idle ? 0.045f : 0.024f))) * envelope * stress;
    base.rotation.z += (std::sin(time * (urgent ? 4.6f : 1.45f) + 0.35f) * (idle ? 0.115f : 0.050f)) * envelope * stress;
    base.rotation.y += (std::sin(time * (urgent ? 3.4f : 0.95f) + 0.2f) * (idle ? 0.090f : 0.038f)) * envelope;
    base.scale.y *= 1.0f + (breath * 0.004f * envelope);
    return base;
}

const StaticMeshInstance* Application::findStaticMeshInstance(const std::string& entityId) const {
    if (entityId.empty()) {
        return nullptr;
    }
    for (const StaticMeshInstance& instance : scene_.staticMeshes()) {
        if (instance.name == entityId) {
            return &instance;
        }
    }
    return nullptr;
}

Transform Application::currentEntityTransform(const StaticMeshInstance& instance) const {
    return animationSystem_.transformFor(instance.name, instance.transform);
}

Transform Application::actionTargetTransform(const SequenceAction& action, Transform current) const {
    if (action.hasPosition) {
        current.position = action.position;
    }
    if (action.hasRotation) {
        current.rotation = action.rotation;
    }
    if (action.hasScale) {
        current.scale = action.scale;
    }
    return current;
}

void Application::setEntityTransformOverride(const SequenceAction& action) {
    if (action.entityId.empty()) {
        Logger::warn("Sequence setEntityTransform action missing entityId");
        return;
    }

    const StaticMeshInstance* instance = findStaticMeshInstance(action.entityId);
    if (instance == nullptr) {
        Logger::warn("Sequence setEntityTransform unknown entityId: " + action.entityId);
        return;
    }

    animationSystem_.setTransform(action.entityId, actionTargetTransform(action, currentEntityTransform(*instance)));
}

void Application::animateEntityTransformOverride(const SequenceAction& action) {
    if (action.entityId.empty()) {
        Logger::warn("Sequence animateEntityTransform action missing entityId");
        return;
    }

    const StaticMeshInstance* instance = findStaticMeshInstance(action.entityId);
    if (instance == nullptr) {
        Logger::warn("Sequence animateEntityTransform unknown entityId: " + action.entityId);
        return;
    }

    const Transform start = currentEntityTransform(*instance);
    const Transform target = actionTargetTransform(action, start);
    std::string easing = action.easing.empty() ? std::string("linear") : action.easing;
    animationSystem_.animateTransform(action.entityId, start, target, action.duration, easing);
}

void Application::clearEntityTransformOverride(const SequenceAction& action) {
    if (action.entityId.empty()) {
        Logger::warn("Sequence clearEntityTransform action missing entityId");
        return;
    }
    if (findStaticMeshInstance(action.entityId) == nullptr) {
        Logger::warn("Sequence clearEntityTransform unknown entityId: " + action.entityId);
        return;
    }
    animationSystem_.clearTransform(action.entityId);
}

void Application::startCharacterPerformance(const SequenceAction& action) {
    if (action.entityId.empty()) {
        Logger::warn("Sequence startCharacterPerformance action missing entityId");
        return;
    }
    if (findStaticMeshInstance(action.entityId) == nullptr) {
        Logger::warn("Sequence startCharacterPerformance unknown entityId: " + action.entityId);
        return;
    }

    const float duration = action.duration > 0.0f ? action.duration : 1.0f;
    characterPerformances_[action.entityId] = {
        .cueId = action.cueId.empty() ? action.audioCue : action.cueId,
        .duration = duration,
        .elapsed = 0.0f,
        .intensity = action.intensity,
    };
}

void Application::stopCharacterPerformance(const SequenceAction& action) {
    if (action.entityId.empty()) {
        Logger::warn("Sequence stopCharacterPerformance action missing entityId");
        return;
    }
    characterPerformances_.erase(action.entityId);
}

void Application::updateCharacterPerformances(float deltaSeconds) {
    const float dt = std::max(deltaSeconds, 0.0f);
    for (auto it = characterPerformances_.begin(); it != characterPerformances_.end();) {
        ActiveCharacterPerformance& performance = it->second;
        performance.elapsed += dt;
        if (performance.elapsed >= performance.duration) {
            it = characterPerformances_.erase(it);
        } else {
            ++it;
        }
    }
}

void Application::evaluateCurrentTrigger() {
    if (!roomManager_.loaded()) {
        return;
    }

    const RoomTrigger* trigger = roomManager_.triggerAt(gameState_.playerPosition);
    if (trigger == nullptr) {
        return;
    }

    InteractionResult result = InteractionSystem::enterTrigger(gameState_, *trigger);
    if (!result.activated) {
        return;
    }

    if (!result.storyNode.empty() && story_.jumpTo(result.storyNode)) {
        story_.restoreState(story_.currentNodeId(), gameState_.identity);
        syncStoryToGameState();
    }
    playAudioCue(trigger->audioCue);
    if (!trigger->sequenceId.empty() && !sequenceManager_.startSequence(trigger->sequenceId, gameState_.flags)) {
        Logger::warn("Trigger sequence did not start: " + trigger->sequenceId);
    }
    Logger::info("Trigger: " + trigger->id);
}

void Application::processRoomEnterEvents() {
    if (!roomManager_.loaded()) {
        return;
    }

    playRoomMusic();

    for (const RoomEnterEvent& event : roomManager_.currentRoom().roomEnterEvents) {
        if (event.once && !event.setFlag.empty() && gameState_.flags.contains(event.setFlag)) {
            continue;
        }

        if (!event.setFlag.empty()) {
            gameState_.flags.insert(event.setFlag);
        }
        playAudioCue(event.audioCue);
        Logger::info("Room enter event: " + event.id);
    }
}

void Application::playRoomMusic() {
    if (!roomManager_.loaded() || roomManager_.currentRoom().musicPath.empty()) {
        return;
    }

    std::filesystem::path path = roomManager_.currentRoom().musicPath;
    if (!path.is_absolute()) {
        path = engineRoot() / path;
    }
    const bool played = audioSystem_.playMusic(path, -1);
    if (!played && audioSystem_.available()) {
        Logger::warn("Room music did not play: " + path.string());
    }
}

void Application::playAudioCue(const std::string& cueId, bool loop) {
    if (cueId.empty() || !roomManager_.loaded()) {
        return;
    }

    const auto& cues = roomManager_.currentRoom().audioCues;
    const auto it = cues.find(cueId);
    if (it == cues.end()) {
        Logger::warn("Missing audio cue in room '" + roomManager_.currentRoom().id + "': " + cueId);
        return;
    }

    std::filesystem::path path = it->second;
    if (!path.is_absolute()) {
        path = engineRoot() / path;
    }
    const bool played = audioSystem_.playOneShot(path, loop ? -1 : 0);
    if (!played && audioSystem_.available()) {
        Logger::warn("Audio cue did not play: " + cueId);
    }
}

bool Application::isStaticMeshVisible(const StaticMeshInstance& instance) const {
    if (sequenceHiddenEntities_.contains(instance.name)) {
        return false;
    }
    if (!instance.visibleWhenFlag.empty() && !gameState_.flags.contains(instance.visibleWhenFlag)) {
        return false;
    }
    if (!instance.hiddenWhenFlag.empty() && gameState_.flags.contains(instance.hiddenWhenFlag)) {
        return false;
    }
    return true;
}

void Application::syncStoryToGameState() {
    if (!story_.loaded()) {
        return;
    }
    gameState_.storyNodeId = story_.currentNodeId();
    gameState_.identity = story_.identity();
}

void Application::syncGameStateToStory() {
    if (!story_.loaded() || gameState_.storyNodeId.empty()) {
        return;
    }
    if (!story_.restoreState(gameState_.storyNodeId, gameState_.identity)) {
        Logger::warn("Unable to restore story state: " + gameState_.storyNodeId);
    }
}

std::filesystem::path Application::engineRoot() const {
    return std::filesystem::path(EXO_ENGINE_ROOT);
}

std::filesystem::path Application::dataRoot() const {
    return engineRoot() / "data";
}

std::filesystem::path Application::savePath() const {
    return engineRoot() / "saves" / "zero_patient_slot_1.json";
}

RenderView Application::makeCurrentView() const {
    const float aspect = window_.height() == 0
        ? 16.0f / 9.0f
        : static_cast<float>(window_.width()) / static_cast<float>(window_.height());

    if (!config_.inspectEntityId.empty()) {
        if (const StaticMeshInstance* instance = findStaticMeshInstance(config_.inspectEntityId)) {
            const Transform transform = currentEntityTransform(*instance);
            const Vec3 target = transform.position + Vec3 {0.0f, 0.62f, 0.0f};
            const Vec3 cameraPosition = target + Vec3 {-1.20f, 0.16f, 1.16f};

            RenderView view;
            view.view = Mat4::lookAt(cameraPosition, target, {0.0f, 1.0f, 0.0f});
            view.projection = Mat4::perspective(0.95f, aspect, 0.03f, 90.0f);
            view.cameraPosition = cameraPosition;
            return view;
        }
    }

    Vec3 cameraPosition = gameState_.playerPosition;
    float yaw = gameState_.playerYaw;
    float pitch = playerPitch_;
    float roll = collapseCameraRoll_;

    if (lyingLimitedLookActive()) {
        const float breath = std::sin(lyingLookTimer_ * 1.55f);
        const float slowBreath = std::sin(lyingLookTimer_ * 0.72f);
        const float vehicleTremor = (std::sin(visualTime_ * 9.4f) * 0.0018f)
            + (std::sin((visualTime_ * 17.6f) + 0.7f) * 0.0009f);
        cameraPosition = lyingAnchorPosition_;
        cameraPosition.x += vehicleTremor * 0.45f;
        cameraPosition.y += (breath * 0.010f) + vehicleTremor;
        cameraPosition.z += slowBreath * 0.006f;
        yaw = lyingBaseYaw_ + lyingYawOffset_;
        pitch = lyingBasePitch_ + lyingPitchOffset_ + (breath * 0.008f);
        roll = (slowBreath * 0.010f) + (vehicleTremor * 1.2f);
    } else if (collapseSequenceState_ == CollapseSequenceState::Dizzy) {
        const float t = smoothStep01(collapseSequenceTimer_ / 3.15f);
        const float sway = std::sin(collapseSequenceTimer_ * 5.4f) * 0.045f * t;
        const float bob = std::sin(collapseSequenceTimer_ * 8.1f) * 0.035f * t;
        const float sinYawBase = std::sin(yaw);
        const float cosYawBase = std::cos(yaw);
        const Vec3 right {cosYawBase, 0.0f, sinYawBase};
        cameraPosition = cameraPosition + (right * sway);
        cameraPosition.y += bob;
        pitch += std::sin(collapseSequenceTimer_ * 3.2f) * 0.045f * t;
    } else if (collapseSequenceState_ == CollapseSequenceState::Falling
        || collapseSequenceState_ == CollapseSequenceState::Blackout) {
        const float rawT = collapseSequenceState_ == CollapseSequenceState::Falling
            ? collapseSequenceTimer_ / 1.30f
            : 1.0f;
        const float t = smoothStep01(rawT);
        const float sinYawBase = std::sin(collapseStartYaw_);
        const float cosYawBase = std::cos(collapseStartYaw_);
        const Vec3 right {cosYawBase, 0.0f, sinYawBase};
        const Vec3 back {-sinYawBase, 0.0f, cosYawBase};
        const Vec3 floorPosition = collapseStartPosition_ + (right * 0.24f) + (back * 0.18f);
        cameraPosition = lerpVec3(collapseStartPosition_, {floorPosition.x, 0.36f, floorPosition.z}, t);
        yaw = collapseStartYaw_ + (0.28f * t);
        pitch = lerpFloat(collapseStartPitch_, -1.04f, t);
    } else if (cameraMode_ == CameraMode::FreeFirstPerson) {
        const float sinYawBase = std::sin(yaw);
        const float cosYawBase = std::cos(yaw);
        const Vec3 right {cosYawBase, 0.0f, sinYawBase};
        const float idleBreath = std::sin(visualTime_ * 1.18f) * 0.0025f;
        const float footBob = std::sin(walkCameraPhase_ * 2.0f) * 0.010f * walkCameraAmount_;
        const float footSway = std::sin(walkCameraPhase_) * 0.006f * walkCameraAmount_;
        cameraPosition = cameraPosition + (right * footSway);
        cameraPosition.y += idleBreath + footBob;
        pitch += std::sin((walkCameraPhase_ * 2.0f) + 0.35f) * 0.0025f * walkCameraAmount_;
        roll += std::sin(walkCameraPhase_) * 0.0065f * walkCameraAmount_;
    }

    const float cosPitch = std::cos(pitch);
    const float sinPitch = std::sin(pitch);
    const float sinYaw = std::sin(yaw);
    const float cosYaw = std::cos(yaw);
    const Vec3 forward {
        sinYaw * cosPitch,
        sinPitch,
        -cosYaw * cosPitch,
    };
    Vec3 side = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
    if (side.length() <= 0.00001f) {
        side = {1.0f, 0.0f, 0.0f};
    }
    const Vec3 baseUp = normalize(cross(side, forward));
    const Vec3 rolledUp = normalize((baseUp * std::cos(roll)) + (side * std::sin(roll)));

    RenderView view;
    view.view = Mat4::lookAt(cameraPosition, cameraPosition + forward, rolledUp);
    view.projection = Mat4::perspective(1.134464f, aspect, 0.05f, 90.0f);
    view.cameraPosition = cameraPosition;
    return view;
}

} // namespace Exo
