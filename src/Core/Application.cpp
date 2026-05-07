#include <ExoEngine/Core/Application.h>

#include <ExoEngine/Assets/GltfLoader.h>
#include <ExoEngine/Assets/ObjImporter.h>
#include <ExoEngine/Core/Logger.h>
#include <ExoEngine/Scene/SceneLoader.h>

#include <SDL2/SDL.h>

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

    Logger::info("Scene loaded: " + scene_.name());
    Logger::info("Fixed camera shots: " + std::to_string(scene_.cameraRig().shots().size()));
    Logger::info("Static mesh slots: " + std::to_string(scene_.staticMeshes().size()));
    Logger::info("Point lights: " + std::to_string(scene_.pointLights().size()));

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
    Logger::info("Player spawn: pos=(0.000, 1.650, 1.500) yaw=0.000 pitch=0.000 (eye height 1.65 m)");
    Logger::info("Walkable bounds (XZ clamp): [-1.850, 1.850]");

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

    if (!debugOverlay_.initialize(window_.nativeHandle(), nullptr)) {
        Logger::warn("Debug overlay is not available");
    }

    sceneMeshHandles_.assign(scene_.staticMeshes().size(), -1);
    const std::filesystem::path engineRoot(EXO_ENGINE_ROOT);
    for (std::size_t i = 0; i < scene_.staticMeshes().size(); ++i) {
        const StaticMeshInstance& instance = scene_.staticMeshes()[i];
        if (instance.meshSource.empty()) {
            continue;
        }
        std::filesystem::path source(instance.meshSource);
        if (!source.is_absolute()) {
            source = engineRoot / source;
        }
        sceneMeshHandles_[i] = renderer_.loadSceneMesh(source);
    }

    if (config_.maxFrames == 0) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }

    Logger::info("Sandbox running. WASD to move, mouse to look, Esc to quit.");

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
            debugOverlay_.handleEvent(event);

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
        debugOverlay_.beginFrame();
        debugOverlay_.drawEngineOverlay(scene_, renderer_.stats());
        debugOverlay_.endFrame();
        window_.swapBuffers();

        if (config_.maxFrames != 0 && renderer_.stats().frameIndex >= config_.maxFrames) {
            running = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    SDL_SetRelativeMouseMode(SDL_FALSE);
    debugOverlay_.shutdown();
    renderer_.shutdown();
    window_.destroy();
    return 0;
}

void Application::updatePlayer(float deltaSeconds, float mouseDeltaX, float mouseDeltaY) {
    constexpr float kMouseSensitivity = 0.0025f;
    constexpr float kMaxPitch = 1.5f;
    constexpr float kWalkSpeed = 2.5f;
    constexpr float kRunMultiplier = 1.8f;
    constexpr float kEyeHeight = 1.65f;
    constexpr float kRoomHalfExtent = 1.85f;

    playerYaw_ += mouseDeltaX * kMouseSensitivity;
    playerPitch_ -= mouseDeltaY * kMouseSensitivity;
    if (playerPitch_ > kMaxPitch) {
        playerPitch_ = kMaxPitch;
    } else if (playerPitch_ < -kMaxPitch) {
        playerPitch_ = -kMaxPitch;
    }

    const std::uint8_t* keys = SDL_GetKeyboardState(nullptr);
    if (keys == nullptr) {
        playerPosition_.y = kEyeHeight;
        return;
    }

    const float sinYaw = std::sin(playerYaw_);
    const float cosYaw = std::cos(playerYaw_);
    const Vec3 forwardXZ {sinYaw, 0.0f, -cosYaw};
    const Vec3 right {cosYaw, 0.0f, sinYaw};

    Vec3 move {0.0f, 0.0f, 0.0f};
    if (keys[SDL_SCANCODE_W] != 0) {
        move = move + forwardXZ;
    }
    if (keys[SDL_SCANCODE_S] != 0) {
        move = move - forwardXZ;
    }
    if (keys[SDL_SCANCODE_D] != 0) {
        move = move + right;
    }
    if (keys[SDL_SCANCODE_A] != 0) {
        move = move - right;
    }

    if (move.length() > 0.001f) {
        const bool running = (keys[SDL_SCANCODE_LSHIFT] != 0) || (keys[SDL_SCANCODE_RSHIFT] != 0);
        const float speed = running ? (kWalkSpeed * kRunMultiplier) : kWalkSpeed;
        const Vec3 step = normalize(move) * (speed * deltaSeconds);
        playerPosition_.x += step.x;
        playerPosition_.z += step.z;
    }

    if (playerPosition_.x > kRoomHalfExtent) {
        playerPosition_.x = kRoomHalfExtent;
    } else if (playerPosition_.x < -kRoomHalfExtent) {
        playerPosition_.x = -kRoomHalfExtent;
    }
    if (playerPosition_.z > kRoomHalfExtent) {
        playerPosition_.z = kRoomHalfExtent;
    } else if (playerPosition_.z < -kRoomHalfExtent) {
        playerPosition_.z = -kRoomHalfExtent;
    }
    playerPosition_.y = kEyeHeight;
}

bool Application::loadStartupScene(bool required) {
    const std::filesystem::path scenePath = std::filesystem::path(EXO_ENGINE_ROOT) / "samples" / "reference_scene.json";

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

RenderView Application::makeCurrentView() const {
    const float cosPitch = std::cos(playerPitch_);
    const float sinPitch = std::sin(playerPitch_);
    const float sinYaw = std::sin(playerYaw_);
    const float cosYaw = std::cos(playerYaw_);

    const Vec3 forward {
        sinYaw * cosPitch,
        sinPitch,
        -cosYaw * cosPitch,
    };
    const Vec3 target = playerPosition_ + forward;

    const float aspect = window_.height() == 0
        ? 16.0f / 9.0f
        : static_cast<float>(window_.width()) / static_cast<float>(window_.height());

    RenderView view;
    view.view = Mat4::lookAt(playerPosition_, target, {0.0f, 1.0f, 0.0f});
    view.projection = Mat4::perspective(1.221730f, aspect, 0.05f, 80.0f);
    view.cameraPosition = playerPosition_;
    return view;
}

} // namespace Exo
