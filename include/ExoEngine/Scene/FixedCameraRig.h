#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <ExoEngine/Math/Mat4.h>
#include <ExoEngine/Math/Vec.h>
#include <ExoEngine/Renderer/Renderer.h>

namespace Exo {

struct Bounds3 {
    Vec3 min {};
    Vec3 max {};

    [[nodiscard]] bool contains(Vec3 point) const;
};

struct FixedCameraShot {
    std::string name;
    Bounds3 activationBounds {};
    Vec3 position {};
    Vec3 target {};
    float fovRadians = 0.7853981f;
    float nearPlane = 0.05f;
    float farPlane = 80.0f;
    std::int32_t priority = 0;
    float blendSeconds = 0.0f;
};

class FixedCameraRig {
public:
    void addShot(FixedCameraShot shot);
    [[nodiscard]] const FixedCameraShot* chooseShot(Vec3 playerPosition) const;
    [[nodiscard]] RenderView makeRenderView(const FixedCameraShot& shot, float aspect) const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] const std::vector<FixedCameraShot>& shots() const;

private:
    std::vector<FixedCameraShot> shots_;
};

} // namespace Exo
