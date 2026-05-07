#include <ExoEngine/Renderer/Texture2D.h>

#include <ExoEngine/Core/Logger.h>

#include <GL/glew.h>
#include <stb_image.h>

#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

namespace Exo {

namespace {

std::vector<unsigned char> readBinaryFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }

    return {
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>(),
    };
}

} // namespace

Texture2D::~Texture2D() {
    destroy();
}

Texture2D::Texture2D(Texture2D&& other) noexcept {
    *this = std::move(other);
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy();
    texture_ = std::exchange(other.texture_, 0);
    width_ = std::exchange(other.width_, 0);
    height_ = std::exchange(other.height_, 0);
    sourcePath_ = std::move(other.sourcePath_);
    return *this;
}

bool Texture2D::loadFromFile(const std::filesystem::path& path, const TextureLoadOptions& options) {
    destroy();

    const std::vector<unsigned char> bytes = readBinaryFile(path);
    if (bytes.empty()) {
        Logger::error("Texture file could not be read: " + path.string());
        return false;
    }

    stbi_set_flip_vertically_on_load(options.flipVertically ? 1 : 0);

    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(
        bytes.data(),
        static_cast<int>(bytes.size()),
        &width_,
        &height_,
        &channels,
        STBI_rgb_alpha
    );

    if (pixels == nullptr) {
        Logger::error("Texture decode failed: " + path.string());
        return false;
    }

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, options.generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    const GLint internalFormat = options.srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internalFormat,
        width_,
        height_,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels
    );

    if (options.generateMipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);

    sourcePath_ = path;
    Logger::info("Texture loaded: " + path.string());
    return true;
}

void Texture2D::destroy() {
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }

    width_ = 0;
    height_ = 0;
    sourcePath_.clear();
}

void Texture2D::bind(std::uint32_t slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, texture_);
}

bool Texture2D::valid() const {
    return texture_ != 0;
}

std::uint32_t Texture2D::handle() const {
    return texture_;
}

std::int32_t Texture2D::width() const {
    return width_;
}

std::int32_t Texture2D::height() const {
    return height_;
}

const std::filesystem::path& Texture2D::sourcePath() const {
    return sourcePath_;
}

} // namespace Exo
