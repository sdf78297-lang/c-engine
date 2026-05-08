#include <ExoEngine/Core/Application.h>

#include <ExoEngine/Assets/GltfLoader.h>
#include <ExoEngine/Assets/ObjImporter.h>
#include <ExoEngine/Core/Logger.h>
#include <ExoEngine/Game/InteractionSystem.h>
#include <ExoEngine/Game/InventorySystem.h>
#include <ExoEngine/Game/SaveSystem.h>
#include <ExoEngine/Scene/SceneLoader.h>

#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
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

} // namespace

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
            + std::to_string(roomManager_.currentRoom().triggers.size()) + " triggers)");
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

    if (!renderer_.initialize(window_.width(), window_.height())) {
        return 1;
    }

    reloadSceneMeshes();

    if (config_.maxFrames == 0) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }

    Logger::info("Runtime running. WASD move, mouse look, Shift run, Tab mouse capture, E interact, F5 save, F9 load, Esc quit.");

    bool running = true;
    auto lastTime = SDL_GetPerformanceCounter();
    const float perfFreq = static_cast<float>(SDL_GetPerformanceFrequency());

    while (running) {
        const auto now = SDL_GetPerformanceCounter();
        const float deltaSeconds = static_cast<float>(now - lastTime) / perfFreq;
        lastTime = now;

        float mouseDeltaX = 0.0f;
        float mouseDeltaY = 0.0f;

        SDL_Event event {};
        while (SDL_PollEvent(&event) != 0) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
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
                                Logger::info("Loaded game state: " + savePath().string());
                            }
                        } else {
                            Logger::error(error);
                        }
                    }
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        renderer_.resize(
                            static_cast<std::uint32_t>(event.window.data1),
                            static_cast<std::uint32_t>(event.window.data2)
                        );
                    }
                    break;
                case SDL_MOUSEMOTION:
                    if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
                        mouseDeltaX += static_cast<float>(event.motion.xrel);
                        mouseDeltaY += static_cast<float>(event.motion.yrel);
                    }
                    break;
                default:
                    break;
            }
        }

        if (config_.maxFrames == 0) {
            updatePlayer(deltaSeconds, mouseDeltaX, mouseDeltaY);
            evaluateCurrentTrigger();
        }

        renderer_.beginFrame(makeCurrentView());
        for (std::size_t i = 0; i < scene_.staticMeshes().size(); ++i) {
            const StaticMeshInstance& instance = scene_.staticMeshes()[i];
            const Mat4 model = Mat4::translate(instance.transform.position)
                * Mat4::rotateY(instance.transform.rotation.y)
                * Mat4::scale(instance.transform.scale);
            if (sceneMeshHandles_[i] >= 0) {
                renderer_.drawSceneMesh(sceneMeshHandles_[i], model);
            }
        }
        renderer_.endFrame();
        window_.swapBuffers();

        if (config_.maxFrames != 0 && renderer_.stats().frameIndex >= config_.maxFrames) {
            running = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    SDL_SetRelativeMouseMode(SDL_FALSE);
    renderer_.shutdown();
    window_.destroy();
    return 0;
}

void Application::updatePlayer(float deltaSeconds, float mouseDeltaX, float mouseDeltaY) {
    constexpr float kMouseSensitivity = 0.0022f;
    constexpr float kMaxPitch = 1.35f;
    constexpr float kWalkSpeed = 2.2f;
    constexpr float kRunMultiplier = 1.65f;
    constexpr float kEyeHeight = 1.65f;

    gameState_.playerYaw += mouseDeltaX * kMouseSensitivity;
    playerPitch_ -= mouseDeltaY * kMouseSensitivity;
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
        gameState_.playerYaw -= 1.8f * deltaSeconds;
    }
    if (keys[SDL_SCANCODE_RIGHT] != 0) {
        gameState_.playerYaw += 1.8f * deltaSeconds;
    }

    const Bounds3 walkBounds = roomManager_.loaded()
        ? roomManager_.currentRoom().walkBounds
        : Bounds3 {{-1.85f, 0.0f, -1.85f}, {1.85f, 2.4f, 1.85f}};

    if (move.length() > 0.001f) {
        const bool running = (keys[SDL_SCANCODE_LSHIFT] != 0) || (keys[SDL_SCANCODE_RSHIFT] != 0);
        const float speed = running ? (kWalkSpeed * kRunMultiplier) : kWalkSpeed;
        gameState_.playerPosition = gameState_.playerPosition + normalize(move) * (speed * deltaSeconds);
    }

    gameState_.playerPosition.x = std::clamp(gameState_.playerPosition.x, walkBounds.min.x, walkBounds.max.x);
    gameState_.playerPosition.z = std::clamp(gameState_.playerPosition.z, walkBounds.min.z, walkBounds.max.z);
    gameState_.playerPosition.y = kEyeHeight;

    currentFocusPrompt_.clear();
    if (roomManager_.loaded()) {
        if (const RoomDoor* door = roomManager_.doorAt(gameState_.playerPosition)) {
            currentFocusPrompt_ = door->prompt.empty() ? door->id : door->prompt;
        } else if (const RoomInteraction* interaction = roomManager_.interactionAt(gameState_.playerPosition)) {
            currentFocusPrompt_ = interaction->prompt.empty() ? interaction->id : interaction->prompt;
        }
    }
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
        scene_ = SceneLoader::loadFromFile(scenePath);
        Logger::info("Room loaded: " + roomManager_.currentRoom().id + " -> " + scenePath.string());
        if (renderer_.stats().frameIndex > 0 || !sceneMeshHandles_.empty()) {
            reloadSceneMeshes();
        }
        return true;
    } catch (const std::exception& error) {
        Logger::error(error.what());
        return !required;
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
    for (std::size_t i = 0; i < scene_.staticMeshes().size(); ++i) {
        const StaticMeshInstance& instance = scene_.staticMeshes()[i];
        if (instance.meshSource.empty()) {
            continue;
        }
        std::filesystem::path source(instance.meshSource);
        if (!source.is_absolute()) {
            source = root / source;
        }
        sceneMeshHandles_[i] = renderer_.loadSceneMesh(source);
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
        Logger::info("Interaction: " + interaction->id);
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
    Logger::info("Trigger: " + trigger->id);
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

    const float cosPitch = std::cos(playerPitch_);
    const float sinPitch = std::sin(playerPitch_);
    const float sinYaw = std::sin(gameState_.playerYaw);
    const float cosYaw = std::cos(gameState_.playerYaw);
    const Vec3 forward {
        sinYaw * cosPitch,
        sinPitch,
        -cosYaw * cosPitch,
    };

    RenderView view;
    view.view = Mat4::lookAt(gameState_.playerPosition, gameState_.playerPosition + forward, {0.0f, 1.0f, 0.0f});
    view.projection = Mat4::perspective(1.134464f, aspect, 0.05f, 90.0f);
    view.cameraPosition = gameState_.playerPosition;
    return view;
}

} // namespace Exo
