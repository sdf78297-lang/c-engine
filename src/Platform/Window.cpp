#include <ExoEngine/Platform/Window.h>

#include <ExoEngine/Core/Logger.h>

#include <GL/glew.h>
#include <SDL2/SDL.h>

#include <utility>

namespace Exo {

Window::~Window() {
    destroy();
}

Window::Window(Window&& other) noexcept {
    *this = std::move(other);
}

Window& Window::operator=(Window&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy();
    window_ = std::exchange(other.window_, nullptr);
    glContext_ = std::exchange(other.glContext_, nullptr);
    width_ = std::exchange(other.width_, 0);
    height_ = std::exchange(other.height_, 0);
    return *this;
}

bool Window::create(const WindowConfig& config) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        Logger::error(std::string("SDL_Init failed: ") + SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    std::uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    if (config.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    window_ = SDL_CreateWindow(
        config.title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        static_cast<int>(config.width),
        static_cast<int>(config.height),
        flags
    );

    if (window_ == nullptr) {
        Logger::error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        SDL_Quit();
        return false;
    }

    glContext_ = SDL_GL_CreateContext(window_);
    if (glContext_ == nullptr) {
        Logger::error(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
        destroy();
        SDL_Quit();
        return false;
    }

    SDL_GL_MakeCurrent(window_, glContext_);
    SDL_GL_SetSwapInterval(config.vsync ? 1 : 0);

    glewExperimental = GL_TRUE;
    const GLenum glewResult = glewInit();
    glGetError();
    if (glewResult != GLEW_OK) {
        Logger::error(reinterpret_cast<const char*>(glewGetErrorString(glewResult)));
        destroy();
        SDL_Quit();
        return false;
    }

    width_ = config.width;
    height_ = config.height;
    Logger::info("SDL/OpenGL window created");
    return true;
}

void Window::destroy() {
    if (glContext_ != nullptr) {
        SDL_GL_DeleteContext(glContext_);
        glContext_ = nullptr;
    }

    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
    width_ = 0;
    height_ = 0;
}

void Window::swapBuffers() {
    if (window_ != nullptr) {
        SDL_GL_SwapWindow(window_);
    }
}

void Window::resize(std::uint32_t width, std::uint32_t height) {
    width_ = width == 0 ? 1 : width;
    height_ = height == 0 ? 1 : height;
}

bool Window::isOpen() const {
    return window_ != nullptr;
}

std::uint32_t Window::width() const {
    return width_;
}

std::uint32_t Window::height() const {
    return height_;
}

SDL_Window* Window::nativeHandle() const {
    return window_;
}

} // namespace Exo
