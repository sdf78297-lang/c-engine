#include <ExoEngine/Debug/DebugOverlay.h>

#include <ExoEngine/Game/GameState.h>
#include <ExoEngine/Game/RoomManager.h>
#include <ExoEngine/Renderer/Renderer.h>
#include <ExoEngine/Scene/FixedCameraRig.h>
#include <ExoEngine/Scene/Scene.h>
#include <ExoEngine/Story/StoryRuntime.h>

#include <cstddef>
#include <string>

#if defined(EXO_DEBUG_UI_WITH_IMGUI)
#if !__has_include(<imgui.h>)
#error "EXO_DEBUG_UI_WITH_IMGUI requires Dear ImGui headers on the include path."
#endif

#include <imgui.h>

#if __has_include(<backends/imgui_impl_opengl3.h>)
#include <backends/imgui_impl_opengl3.h>
#elif __has_include(<imgui_impl_opengl3.h>)
#include <imgui_impl_opengl3.h>
#else
#error "EXO_DEBUG_UI_WITH_IMGUI requires imgui_impl_opengl3.h."
#endif

#if __has_include(<backends/imgui_impl_sdl2.h>)
#include <backends/imgui_impl_sdl2.h>
#elif __has_include(<imgui_impl_sdl2.h>)
#include <imgui_impl_sdl2.h>
#else
#error "EXO_DEBUG_UI_WITH_IMGUI requires imgui_impl_sdl2.h."
#endif

#include <SDL2/SDL.h>

#define EXO_DEBUG_OVERLAY_HAS_IMGUI 1
#else
#define EXO_DEBUG_OVERLAY_HAS_IMGUI 0
#endif

namespace Exo {

#if EXO_DEBUG_OVERLAY_HAS_IMGUI
namespace {

constexpr const char* glslVersion = "#version 330";
constexpr float radiansToDegrees = 57.2957795f;

ImGuiContext* asImGuiContext(void* context) {
    return static_cast<ImGuiContext*>(context);
}

void setCurrentContext(void* context) {
    if (context != nullptr) {
        ImGui::SetCurrentContext(asImGuiContext(context));
    }
}

void applyDebugStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.058f, 0.063f, 0.96f);
    colors[ImGuiCol_Border] = ImVec4(0.24f, 0.25f, 0.27f, 0.90f);
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.91f, 0.89f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.48f, 0.50f, 0.52f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.24f, 0.22f, 0.90f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.34f, 0.30f, 0.95f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.34f, 0.44f, 0.37f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.13f, 0.14f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.28f, 0.24f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.22f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.32f, 0.27f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.36f, 0.48f, 0.39f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.09f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.11f, 0.13f, 0.12f, 1.00f);
}

void loadReadableFonts(ImGuiIO& io) {
    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 2;
    if (io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, &config, io.Fonts->GetGlyphRangesCyrillic()) == nullptr) {
        io.Fonts->AddFontDefault();
    }
}

void drawMetric(const char* label, const char* value) {
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(140.0f);
    ImGui::TextUnformatted(value);
}

void drawVec3(const char* label, Vec3 value) {
    ImGui::Text("%s  %.2f, %.2f, %.2f", label, value.x, value.y, value.z);
}

void drawTransform(const Transform& transform) {
    drawVec3("Position", transform.position);
    drawVec3("Rotation", transform.rotation);
    drawVec3("Scale", transform.scale);
}

void drawSceneSummary(const Scene& scene) {
    ImGui::TextDisabled("Scene");
    ImGui::SameLine(140.0f);
    ImGui::TextUnformatted(scene.name().c_str());

    ImGui::TextDisabled("Static meshes");
    ImGui::SameLine(140.0f);
    ImGui::Text("%llu", static_cast<unsigned long long>(scene.staticMeshes().size()));

    ImGui::TextDisabled("Point lights");
    ImGui::SameLine(140.0f);
    ImGui::Text("%llu", static_cast<unsigned long long>(scene.pointLights().size()));

    ImGui::TextDisabled("Camera shots");
    ImGui::SameLine(140.0f);
    ImGui::Text("%llu", static_cast<unsigned long long>(scene.cameraRig().shots().size()));
}

void drawStaticMeshes(const Scene& scene) {
    if (!ImGui::CollapsingHeader("Static Meshes", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const auto& meshes = scene.staticMeshes();
    if (meshes.empty()) {
        ImGui::TextDisabled("No static meshes registered.");
        return;
    }

    for (std::size_t index = 0; index < meshes.size(); ++index) {
        const StaticMeshInstance& mesh = meshes[index];
        ImGui::PushID(static_cast<int>(index));
        const char* label = mesh.name.empty() ? "unnamed mesh" : mesh.name.c_str();
        if (ImGui::TreeNode(label)) {
            drawMetric("Mesh asset", mesh.meshAsset.empty() ? "<none>" : mesh.meshAsset.c_str());
            drawMetric("Material asset", mesh.materialAsset.empty() ? "<none>" : mesh.materialAsset.c_str());
            drawMetric("Mesh source", mesh.meshSource.empty() ? "<none>" : mesh.meshSource.c_str());
            drawTransform(mesh.transform);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void drawPointLights(const Scene& scene) {
    if (!ImGui::CollapsingHeader("Point Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const auto& lights = scene.pointLights();
    if (lights.empty()) {
        ImGui::TextDisabled("No point lights registered.");
        return;
    }

    for (std::size_t index = 0; index < lights.size(); ++index) {
        const PointLight& light = lights[index];
        ImGui::PushID(static_cast<int>(index));
        if (ImGui::TreeNode("Point light")) {
            drawVec3("Position", light.position);
            ImGui::Text("Color     %.2f, %.2f, %.2f", light.color.x, light.color.y, light.color.z);
            ImGui::Text("Radius    %.2f", light.radius);
            ImGui::Text("Intensity %.2f", light.intensity);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void drawCameraRig(const Scene& scene) {
    if (!ImGui::CollapsingHeader("Fixed Camera Rig", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const auto& shots = scene.cameraRig().shots();
    if (shots.empty()) {
        ImGui::TextDisabled("No fixed camera shots registered.");
        return;
    }

    for (std::size_t index = 0; index < shots.size(); ++index) {
        const FixedCameraShot& shot = shots[index];
        ImGui::PushID(static_cast<int>(index));
        const char* label = shot.name.empty() ? "unnamed shot" : shot.name.c_str();
        if (ImGui::TreeNode(label)) {
            drawVec3("Position", shot.position);
            drawVec3("Target", shot.target);
            drawVec3("Bounds min", shot.activationBounds.min);
            drawVec3("Bounds max", shot.activationBounds.max);
            ImGui::Text("FOV       %.1f deg", shot.fovRadians * radiansToDegrees);
            ImGui::Text("Near/Far  %.2f / %.2f", shot.nearPlane, shot.farPlane);
            ImGui::Text("Priority  %d", shot.priority);
            ImGui::Text("Blend     %.2f s", shot.blendSeconds);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void drawWrappedParagraph(const std::string& text) {
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
}

} // namespace
#endif

DebugOverlay::~DebugOverlay() {
    shutdown();
}

bool DebugOverlay::initialize(SDL_Window* window, void* glContext) {
#if EXO_DEBUG_OVERLAY_HAS_IMGUI
    if (initialized_) {
        return true;
    }

    void* resolvedContext = glContext;
    if (resolvedContext == nullptr) {
        resolvedContext = SDL_GL_GetCurrentContext();
    }

    if (window == nullptr || resolvedContext == nullptr) {
        return false;
    }

    IMGUI_CHECKVERSION();
    imguiContext_ = ImGui::CreateContext();
    if (imguiContext_ == nullptr) {
        return false;
    }

    setCurrentContext(imguiContext_);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    loadReadableFonts(io);
    applyDebugStyle();

    if (!ImGui_ImplSDL2_InitForOpenGL(window, resolvedContext)) {
        ImGui::DestroyContext(asImGuiContext(imguiContext_));
        imguiContext_ = nullptr;
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init(glslVersion)) {
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext(asImGuiContext(imguiContext_));
        imguiContext_ = nullptr;
        return false;
    }

    window_ = window;
    glContext_ = resolvedContext;
    initialized_ = true;
    frameOpen_ = false;
    return true;
#else
    window_ = window;
    glContext_ = glContext;
    imguiContext_ = nullptr;
    initialized_ = false;
    frameOpen_ = false;
    return false;
#endif
}

void DebugOverlay::shutdown() {
#if EXO_DEBUG_OVERLAY_HAS_IMGUI
    if (imguiContext_ != nullptr) {
        setCurrentContext(imguiContext_);
        if (initialized_) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL2_Shutdown();
        }
        ImGui::DestroyContext(asImGuiContext(imguiContext_));
    }
#endif

    window_ = nullptr;
    glContext_ = nullptr;
    imguiContext_ = nullptr;
    initialized_ = false;
    frameOpen_ = false;
}

void DebugOverlay::handleEvent(const SDL_Event& event) {
#if EXO_DEBUG_OVERLAY_HAS_IMGUI
    if (!initialized_) {
        return;
    }

    setCurrentContext(imguiContext_);
    ImGui_ImplSDL2_ProcessEvent(&event);
#else
    (void)event;
#endif
}

void DebugOverlay::beginFrame() {
#if EXO_DEBUG_OVERLAY_HAS_IMGUI
    if (!initialized_ || frameOpen_) {
        return;
    }

    setCurrentContext(imguiContext_);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    frameOpen_ = true;
#endif
}

void DebugOverlay::drawEngineOverlay(const Scene& scene, const RenderStats& stats) {
#if EXO_DEBUG_OVERLAY_HAS_IMGUI
    if (!initialized_ || !frameOpen_) {
        return;
    }

    setCurrentContext(imguiContext_);

    const ImGuiIO& io = ImGui::GetIO();
    const float frameMs = io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f;

    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 560.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("ExoEngine Debug")) {
        ImGui::Text("Frame %llu", static_cast<unsigned long long>(stats.frameIndex));
        ImGui::SameLine();
        ImGui::TextDisabled("%.1f FPS / %.2f ms", io.Framerate, frameMs);

        ImGui::Separator();
        ImGui::TextDisabled("Renderer");
        ImGui::Text("Draw calls %u", stats.drawCalls);

        ImGui::Separator();
        drawSceneSummary(scene);

        ImGui::Separator();
        drawStaticMeshes(scene);
        drawPointLights(scene);
        drawCameraRig(scene);
    }

    ImGui::End();
#else
    (void)scene;
    (void)stats;
#endif
}

void DebugOverlay::drawStoryOverlay(const StoryRuntime& story) {
#if EXO_DEBUG_OVERLAY_HAS_IMGUI
    if (!initialized_ || !frameOpen_) {
        return;
    }

    setCurrentContext(imguiContext_);
    ImGui::SetNextWindowPos(ImVec2(452.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520.0f, 560.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("НУЛЕВОЙ ПАЦИЕНТ")) {
        if (!story.loaded()) {
            ImGui::TextWrapped("Story runtime is not loaded.");
            ImGui::TextWrapped("%s", story.lastError().c_str());
            ImGui::End();
            return;
        }

        const StoryNode& node = story.currentNode();
        ImGui::TextUnformatted(story.title().c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("Я: %d%%", story.identity());
        ImGui::ProgressBar(static_cast<float>(story.identity()) / 100.0f, ImVec2(-1.0f, 8.0f), "");

        ImGui::Separator();
        ImGui::TextUnformatted(node.title.c_str());
        if (!node.location.empty()) {
            ImGui::TextDisabled("%s", node.location.c_str());
        }

        ImGui::Spacing();
        for (const std::string& paragraph : node.body) {
            drawWrappedParagraph(paragraph);
            ImGui::Spacing();
        }

        if (!node.choices.empty()) {
            ImGui::Separator();
            ImGui::TextDisabled("Выбор: нажми 1, 2 или 3");
            for (std::size_t i = 0; i < node.choices.size(); ++i) {
                const StoryChoice& choice = node.choices[i];
                const std::string label = std::to_string(i + 1) + ". " + choice.label;
                drawWrappedParagraph(label);
            }
        } else if (node.ending) {
            ImGui::Separator();
            ImGui::TextDisabled("Концовка зафиксирована.");
        }
    }

    ImGui::End();
#else
    (void)story;
#endif
}

void DebugOverlay::drawGameOverlay(const RoomManager& roomManager, const GameState& state, const std::string& focusPrompt) {
#if EXO_DEBUG_OVERLAY_HAS_IMGUI
    if (!initialized_ || !frameOpen_) {
        return;
    }

    setCurrentContext(imguiContext_);
    ImGui::SetNextWindowPos(ImVec2(984.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 300.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Gameplay Runtime")) {
        ImGui::TextDisabled("Room");
        ImGui::SameLine(110.0f);
        if (roomManager.loaded()) {
            ImGui::TextUnformatted(roomManager.currentRoom().id.c_str());
        } else {
            ImGui::TextUnformatted("<fallback scene>");
        }

        ImGui::TextDisabled("Spawn");
        ImGui::SameLine(110.0f);
        ImGui::TextUnformatted(state.spawnId.c_str());

        ImGui::TextDisabled("Player");
        ImGui::SameLine(110.0f);
        ImGui::Text("%.2f, %.2f, %.2f", state.playerPosition.x, state.playerPosition.y, state.playerPosition.z);

        ImGui::TextDisabled("Yaw");
        ImGui::SameLine(110.0f);
        ImGui::Text("%.2f", state.playerYaw);

        ImGui::TextDisabled("Inventory");
        ImGui::SameLine(110.0f);
        ImGui::Text("%llu", static_cast<unsigned long long>(state.inventory.size()));

        ImGui::TextDisabled("Flags");
        ImGui::SameLine(110.0f);
        ImGui::Text("%llu", static_cast<unsigned long long>(state.flags.size()));

        ImGui::Separator();
        if (!focusPrompt.empty()) {
            ImGui::TextWrapped("E: %s", focusPrompt.c_str());
        } else {
            ImGui::TextDisabled("No interaction in range.");
        }
        ImGui::TextDisabled("F5 save / F9 load");
    }

    ImGui::End();
#else
    (void)roomManager;
    (void)state;
    (void)focusPrompt;
#endif
}

void DebugOverlay::endFrame() {
#if EXO_DEBUG_OVERLAY_HAS_IMGUI
    if (!initialized_ || !frameOpen_) {
        return;
    }

    setCurrentContext(imguiContext_);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    frameOpen_ = false;
#endif
}

bool DebugOverlay::isInitialized() const {
    return initialized_;
}

} // namespace Exo
