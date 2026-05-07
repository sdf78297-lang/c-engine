#pragma once

#include <cstdint>
#include <string>

struct SDL_Window;

namespace Exo {

struct WindowConfig {
    std::string title = "ExoEngine Sandbox";
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    bool resizable = true;
    bool vsync = true;
};

class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    bool create(const WindowConfig& config);
    void destroy();
    void swapBuffers();

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] std::uint32_t width() const;
    [[nodiscard]] std::uint32_t height() const;
    [[nodiscard]] SDL_Window* nativeHandle() const;

private:
    SDL_Window* window_ = nullptr;
    void* glContext_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
};

} // namespace Exo
