#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace Exo {

struct TextureLoadOptions {
    bool flipVertically = true;
    bool generateMipmaps = true;
    bool srgb = true;
};

class Texture2D {
public:
    Texture2D() = default;
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;

    [[nodiscard]] bool loadFromFile(const std::filesystem::path& path, const TextureLoadOptions& options = {});
    void destroy();
    void bind(std::uint32_t slot = 0) const;

    [[nodiscard]] bool valid() const;
    [[nodiscard]] std::uint32_t handle() const;
    [[nodiscard]] std::int32_t width() const;
    [[nodiscard]] std::int32_t height() const;
    [[nodiscard]] const std::filesystem::path& sourcePath() const;

private:
    std::uint32_t texture_ = 0;
    std::int32_t width_ = 0;
    std::int32_t height_ = 0;
    std::filesystem::path sourcePath_;
};

} // namespace Exo
