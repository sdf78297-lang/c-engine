#include <ExoEngine/Game/CameraDirector.h>

#include <ExoEngine/Math/Mat4.h>

#include <cmath>

namespace Exo {

RenderView CameraDirector::makeView(const Scene& scene, Vec3 playerPosition, float playerYaw, float aspect) const {
    if (!scene.cameraRig().empty()) {
        if (const FixedCameraShot* shot = scene.cameraRig().chooseShot(playerPosition)) {
            return scene.cameraRig().makeRenderView(*shot, aspect);
        }
    }

    const Vec3 forward {std::sin(playerYaw), 0.0f, -std::cos(playerYaw)};
    const Vec3 target = playerPosition + forward;

    RenderView view;
    view.view = Mat4::lookAt(playerPosition, target, {0.0f, 1.0f, 0.0f});
    view.projection = Mat4::perspective(1.221730f, aspect, 0.05f, 80.0f);
    view.cameraPosition = playerPosition;
    return view;
}

} // namespace Exo
