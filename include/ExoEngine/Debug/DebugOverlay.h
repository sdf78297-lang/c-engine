#pragma once

union SDL_Event;
struct SDL_Window;

namespace Exo {

class Scene;
class StoryRuntime;
struct RenderStats;

class DebugOverlay {
public:
    DebugOverlay() = default;
    ~DebugOverlay();

    DebugOverlay(const DebugOverlay&) = delete;
    DebugOverlay& operator=(const DebugOverlay&) = delete;

    bool initialize(SDL_Window* window, void* glContext);
    void shutdown();

    void handleEvent(const SDL_Event& event);
    void beginFrame();
    void drawEngineOverlay(const Scene& scene, const RenderStats& stats);
    void drawStoryOverlay(const StoryRuntime& story);
    void endFrame();

    [[nodiscard]] bool isInitialized() const;

private:
    SDL_Window* window_ = nullptr;
    void* glContext_ = nullptr;
    void* imguiContext_ = nullptr;
    bool initialized_ = false;
    bool frameOpen_ = false;
};

} // namespace Exo
