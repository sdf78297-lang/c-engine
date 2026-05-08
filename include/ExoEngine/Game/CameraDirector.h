#pragma once

#include <ExoEngine/Math/Vec.h>
#include <ExoEngine/Renderer/Renderer.h>
#include <ExoEngine/Scene/Scene.h>

namespace Exo {

class CameraDirector {
public:
    [[nodiscard]] RenderView makeView(const Scene& scene, Vec3 playerPosition, float playerYaw, float aspect) const;
};

} // namespace Exo
