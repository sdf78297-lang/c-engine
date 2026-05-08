#include <ExoEngine/UI/HtmlMenu.h>

#include <ExoEngine/Core/Logger.h>

#include <algorithm>
#include <filesystem>
#include <string>

#if EXO_ENABLE_HTML_UI
#include <AppCore/CAPI.h>
#include <Ultralight/CAPI.h>

#include <GL/glew.h>
#include <SDL2/SDL.h>

#include <cstddef>
#include <vector>
#else
#include <SDL2/SDL.h>
#endif

namespace Exo {

#if EXO_ENABLE_HTML_UI
namespace {

constexpr const char* kMenuVertexShader = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

out vec2 vUV;

void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)glsl";

constexpr const char* kMenuFragmentShader = R"glsl(
#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    FragColor = texture(uTexture, vUV);
}
)glsl";

bool gPlatformReady = false;

std::string canonicalGenericPath(const std::filesystem::path& path) {
    std::error_code ec;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
    return (ec ? path : canonical).generic_string();
}

ULString makeUlString(const std::string& value) {
    return ulCreateStringUTF8(value.data(), value.size());
}

std::uint32_t compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_TRUE) {
        return shader;
    }

    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    Logger::error("HTML menu shader compile failed: " + log);
    glDeleteShader(shader);
    return 0;
}

std::uint32_t linkProgram(std::uint32_t vertexShader, std::uint32_t fragmentShader) {
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == GL_TRUE) {
        return program;
    }

    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    glGetProgramInfoLog(program, length, nullptr, log.data());
    Logger::error("HTML menu shader link failed: " + log);
    glDeleteProgram(program);
    return 0;
}

void onFinishLoading(void* userData, ULView, unsigned long long, bool isMainFrame, ULString) {
    if (!isMainFrame || userData == nullptr) {
        return;
    }
    auto* menu = static_cast<HtmlMenu*>(userData);
    menu->evaluateScript("var exoFirst=document.querySelector('.menu-item:not([disabled])'); if(exoFirst) exoFirst.focus();");
}

void onDomReady(void* userData, ULView, unsigned long long, bool isMainFrame, ULString) {
    if (!isMainFrame || userData == nullptr) {
        return;
    }
    auto* menu = static_cast<HtmlMenu*>(userData);
    menu->evaluateScript("var exoFirst=document.querySelector('.menu-item:not([disabled])'); if(exoFirst) exoFirst.focus();");
}

void onChangeUrl(void* userData, ULView, ULString url) {
    if (userData == nullptr || url == nullptr) {
        return;
    }
    auto* menu = static_cast<HtmlMenu*>(userData);
    menu->handleUrlChanged(ulStringGetData(url));
}

ULMouseButton toUlButton(std::uint8_t button) {
    switch (button) {
        case SDL_BUTTON_LEFT: return kMouseButton_Left;
        case SDL_BUTTON_MIDDLE: return kMouseButton_Middle;
        case SDL_BUTTON_RIGHT: return kMouseButton_Right;
        default: return kMouseButton_None;
    }
}

const char* keyName(SDL_Keycode key) {
    switch (key) {
        case SDLK_UP: return "ArrowUp";
        case SDLK_DOWN: return "ArrowDown";
        case SDLK_LEFT: return "ArrowLeft";
        case SDLK_RIGHT: return "ArrowRight";
        case SDLK_RETURN:
        case SDLK_KP_ENTER: return "Enter";
        case SDLK_ESCAPE: return "Escape";
        default: return nullptr;
    }
}

std::string dispatchKeyScript(const char* key) {
    return std::string("document.dispatchEvent(new KeyboardEvent('keydown',{key:'")
        + key + "',bubbles:true,cancelable:true}));";
}

} // namespace
#endif

HtmlMenu::~HtmlMenu() {
    shutdown();
}

bool HtmlMenu::initialize(const Config& config) {
#if EXO_ENABLE_HTML_UI
    if (initialized_) {
        return true;
    }

    const std::filesystem::path htmlPath = config.engineRoot / config.htmlPath;
    if (!std::filesystem::exists(htmlPath)) {
        Logger::warn("HTML menu is not available: " + htmlPath.string());
        return false;
    }

    width_ = std::max(config.width, 1u);
    height_ = std::max(config.height, 1u);

    if (!createGraphicsResources()) {
        shutdown();
        return false;
    }

    if (!gPlatformReady) {
        ULString baseDir = makeUlString(canonicalGenericPath(config.engineRoot));
        ulEnablePlatformFileSystem(baseDir);
        ulDestroyString(baseDir);

        ulEnablePlatformFontLoader();

        const std::filesystem::path logPath = config.engineRoot / "build" / "ultralight.log";
        ULString log = makeUlString(canonicalGenericPath(logPath));
        ulEnableDefaultLogger(log);
        ulDestroyString(log);

        gPlatformReady = true;
    }

    ULConfig ulConfig = ulCreateConfig();
    const std::filesystem::path resourcePath = config.ultralightResourcePath.empty()
        ? (config.engineRoot / "local_deps" / "ultralight-sdk" / "resources")
        : config.ultralightResourcePath;
    std::string resourcePrefix = canonicalGenericPath(resourcePath);
    if (!resourcePrefix.empty() && resourcePrefix.back() != '/') {
        resourcePrefix.push_back('/');
    }
    ULString resources = makeUlString(resourcePrefix);
    ulConfigSetResourcePathPrefix(ulConfig, resources);
    ulDestroyString(resources);
    ulConfigSetAnimationTimerDelay(ulConfig, 1.0 / 60.0);
    ulConfigSetForceRepaint(ulConfig, true);

    renderer_ = ulCreateRenderer(ulConfig);
    ulDestroyConfig(ulConfig);
    if (renderer_ == nullptr) {
        Logger::error("Failed to create Ultralight renderer");
        shutdown();
        return false;
    }

    ULViewConfig viewConfig = ulCreateViewConfig();
    ulViewConfigSetIsAccelerated(viewConfig, false);
    ulViewConfigSetInitialFocus(viewConfig, true);
    ulViewConfigSetEnableImages(viewConfig, true);
    ulViewConfigSetEnableJavaScript(viewConfig, true);
    view_ = ulCreateView(static_cast<ULRenderer>(renderer_), width_, height_, viewConfig, nullptr);
    ulDestroyViewConfig(viewConfig);
    if (view_ == nullptr) {
        Logger::error("Failed to create Ultralight menu view");
        shutdown();
        return false;
    }

    ulViewSetFinishLoadingCallback(static_cast<ULView>(view_), onFinishLoading, this);
    ulViewSetDOMReadyCallback(static_cast<ULView>(view_), onDomReady, this);
    ulViewSetChangeURLCallback(static_cast<ULView>(view_), onChangeUrl, this);
    ulViewFocus(static_cast<ULView>(view_));

    std::string relativeUrl = "file:///" + config.htmlPath.generic_string();
    ULString url = makeUlString(relativeUrl);
    ulViewLoadURL(static_cast<ULView>(view_), url);
    ulDestroyString(url);

    initialized_ = true;
    Logger::info("HTML menu loaded through Ultralight: " + relativeUrl);
    return true;
#else
    (void)config;
    Logger::warn("HTML menu requested, but EXO_ENABLE_HTML_UI is OFF");
    return false;
#endif
}

void HtmlMenu::shutdown() {
#if EXO_ENABLE_HTML_UI
    if (view_ != nullptr) {
        ulDestroyView(static_cast<ULView>(view_));
        view_ = nullptr;
    }
    if (renderer_ != nullptr) {
        ulDestroyRenderer(static_cast<ULRenderer>(renderer_));
        renderer_ = nullptr;
    }
    destroyGraphicsResources();
#endif
    initialized_ = false;
    startRequested_ = false;
    quitRequested_ = false;
}

void HtmlMenu::resize(std::uint32_t width, std::uint32_t height) {
    width_ = std::max(width, 1u);
    height_ = std::max(height, 1u);
#if EXO_ENABLE_HTML_UI
    if (!initialized_) {
        return;
    }
    if (view_ != nullptr) {
        ulViewResize(static_cast<ULView>(view_), width_, height_);
    }
    if (texture_ != 0) {
        glBindTexture(GL_TEXTURE_2D, texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_), 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
        textureUploaded_ = false;
    }
#endif
}

void HtmlMenu::handleEvent(const SDL_Event& event) {
#if EXO_ENABLE_HTML_UI
    if (!initialized_ || view_ == nullptr) {
        return;
    }

    switch (event.type) {
        case SDL_MOUSEMOTION: {
            ULMouseEvent evt = ulCreateMouseEvent(kMouseEventType_MouseMoved, event.motion.x, event.motion.y, kMouseButton_None);
            ulViewFireMouseEvent(static_cast<ULView>(view_), evt);
            ulDestroyMouseEvent(evt);
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            const ULMouseEventType type = event.type == SDL_MOUSEBUTTONDOWN ? kMouseEventType_MouseDown : kMouseEventType_MouseUp;
            ULMouseEvent evt = ulCreateMouseEvent(type, event.button.x, event.button.y, toUlButton(event.button.button));
            ulViewFireMouseEvent(static_cast<ULView>(view_), evt);
            ulDestroyMouseEvent(evt);
            break;
        }
        case SDL_KEYDOWN: {
            if (const char* key = keyName(event.key.keysym.sym)) {
                evaluateScript(dispatchKeyScript(key));
            }
            break;
        }
        default:
            break;
    }
#else
    (void)event;
#endif
}

void HtmlMenu::update() {
#if EXO_ENABLE_HTML_UI
    if (!initialized_ || renderer_ == nullptr) {
        return;
    }
    ulUpdate(static_cast<ULRenderer>(renderer_));
    ulRender(static_cast<ULRenderer>(renderer_));
#endif
}

void HtmlMenu::render() {
#if EXO_ENABLE_HTML_UI
    if (!initialized_) {
        return;
    }
    uploadSurfaceToTexture();
    if (texture_ == 0 || shader_ == 0 || vao_ == 0 || !textureUploaded_) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(shader_);

    const GLint texLoc = glGetUniformLocation(shader_, "uTexture");
    glUniform1i(texLoc, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
#endif
}

bool HtmlMenu::isActive() const {
    return initialized_;
}

bool HtmlMenu::startRequested() const {
    return startRequested_;
}

bool HtmlMenu::quitRequested() const {
    return quitRequested_;
}

void HtmlMenu::clearRequests() {
    startRequested_ = false;
    quitRequested_ = false;
}

#if EXO_ENABLE_HTML_UI
bool HtmlMenu::createGraphicsResources() {
    const std::uint32_t vertexShader = compileShader(GL_VERTEX_SHADER, kMenuVertexShader);
    if (vertexShader == 0) {
        return false;
    }

    const std::uint32_t fragmentShader = compileShader(GL_FRAGMENT_SHADER, kMenuFragmentShader);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    shader_ = linkProgram(vertexShader, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    if (shader_ == 0) {
        return false;
    }

    const float vertices[] = {
        -1.0f,  1.0f, 0.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
    };

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_), 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    return vao_ != 0 && vbo_ != 0 && texture_ != 0;
}

void HtmlMenu::destroyGraphicsResources() {
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    if (shader_ != 0) {
        glDeleteProgram(shader_);
        shader_ = 0;
    }
    textureUploaded_ = false;
}

void HtmlMenu::uploadSurfaceToTexture() {
    if (view_ == nullptr || texture_ == 0) {
        return;
    }

    ULSurface surface = ulViewGetSurface(static_cast<ULView>(view_));
    if (surface == nullptr) {
        return;
    }

    const ULIntRect dirty = ulSurfaceGetDirtyBounds(surface);
    if (textureUploaded_ && ulIntRectIsEmpty(dirty)) {
        return;
    }

    void* pixels = ulSurfaceLockPixels(surface);
    if (pixels == nullptr) {
        return;
    }

    const unsigned int rowBytes = ulSurfaceGetRowBytes(surface);
    const unsigned int surfaceWidth = ulSurfaceGetWidth(surface);
    const unsigned int surfaceHeight = ulSurfaceGetHeight(surface);

    glBindTexture(GL_TEXTURE_2D, texture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(rowBytes / 4));
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        static_cast<GLsizei>(surfaceWidth),
        static_cast<GLsizei>(surfaceHeight),
        GL_BGRA,
        GL_UNSIGNED_BYTE,
        pixels
    );
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    ulSurfaceUnlockPixels(surface);
    ulSurfaceClearDirtyBounds(surface);
    textureUploaded_ = true;
}

void HtmlMenu::evaluateScript(const char* script) {
    if (script == nullptr || view_ == nullptr) {
        return;
    }
    evaluateScript(std::string(script));
}

void HtmlMenu::evaluateScript(const std::string& script) {
    if (script.empty() || view_ == nullptr) {
        return;
    }
    ULString js = makeUlString(script);
    ULString exception = nullptr;
    ulViewEvaluateScript(static_cast<ULView>(view_), js, &exception);
    if (exception != nullptr && !ulStringIsEmpty(exception)) {
        Logger::warn("HTML menu script warning: " + std::string(ulStringGetData(exception)));
    }
    ulDestroyString(js);
}

void HtmlMenu::handleUrlChanged(const char* url) {
    if (url == nullptr) {
        return;
    }
    const std::string value(url);
    if (value.rfind("exo://start-game", 0) == 0) {
        startRequested_ = true;
    } else if (value.rfind("exo://quit-game", 0) == 0) {
        quitRequested_ = true;
    }
}
#endif

} // namespace Exo
