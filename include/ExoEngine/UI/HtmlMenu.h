#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

union SDL_Event;

namespace Exo {

class HtmlMenu {
public:
    struct SettingChange {
        std::string key;
        float value = 1.0f;
    };

    struct Config {
        std::filesystem::path engineRoot;
        std::filesystem::path htmlPath;
        std::filesystem::path ultralightResourcePath;
        std::string initialScreen = "screen-menu";
        bool pauseOverlay = false;
        std::uint32_t width = 1280;
        std::uint32_t height = 720;
    };

    HtmlMenu() = default;
    ~HtmlMenu();

    HtmlMenu(const HtmlMenu&) = delete;
    HtmlMenu& operator=(const HtmlMenu&) = delete;

    [[nodiscard]] bool initialize(const Config& config);
    void shutdown();
    void resize(std::uint32_t width, std::uint32_t height);
    void handleEvent(const SDL_Event& event);
    void update();
    void render();

    [[nodiscard]] bool isActive() const;
    [[nodiscard]] bool startRequested() const;
    [[nodiscard]] bool quitRequested() const;
    [[nodiscard]] bool resumeRequested() const;
    [[nodiscard]] std::optional<SettingChange> takeSettingChange();
    void clearRequests();

#if EXO_ENABLE_HTML_UI
    void evaluateScript(const char* script);
    void evaluateScript(const std::string& script);
    void handleUrlChanged(const char* url);
    void handleBridgeMessage(const char* message);
#endif

private:
    bool initialized_ = false;
    bool startRequested_ = false;
    bool quitRequested_ = false;
    bool resumeRequested_ = false;
    std::optional<SettingChange> pendingSetting_;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint32_t lastMouseMoveMs_ = 0;
    std::uint32_t lastUpdateMs_ = 0;
    bool needsImmediateUpdate_ = true;

#if EXO_ENABLE_HTML_UI
    void* renderer_ = nullptr;
    void* view_ = nullptr;
    std::uint32_t shader_ = 0;
    std::uint32_t vao_ = 0;
    std::uint32_t vbo_ = 0;
    std::uint32_t texture_ = 0;
    bool textureUploaded_ = false;

    [[nodiscard]] bool createGraphicsResources();
    void destroyGraphicsResources();
    void uploadSurfaceToTexture();
#endif
};

} // namespace Exo
