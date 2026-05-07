#include <ExoEngine/Scene/FixedCameraRig.h>

#include <ExoEngine/Renderer/Renderer.h>

#include <algorithm>
#include <utility>

namespace Exo {

bool Bounds3::contains(Vec3 point) const {
    return point.x >= min.x && point.x <= max.x
        && point.y >= min.y && point.y <= max.y
        && point.z >= min.z && point.z <= max.z;
}

void FixedCameraRig::addShot(FixedCameraShot shot) {
    shots_.push_back(std::move(shot));
    std::ranges::stable_sort(shots_, [](const FixedCameraShot& lhs, const FixedCameraShot& rhs) {
        return lhs.priority > rhs.priority;
    });
}

const FixedCameraShot* FixedCameraRig::chooseShot(Vec3 playerPosition) const {
    for (const FixedCameraShot& shot : shots_) {
        if (shot.activationBounds.contains(playerPosition)) {
            return &shot;
        }
    }

    if (!shots_.empty()) {
        return &shots_.front();
    }

    return nullptr;
}

RenderView FixedCameraRig::makeRenderView(const FixedCameraShot& shot, float aspect) const {
    RenderView view;
    view.cameraPosition = shot.position;
    view.view = Mat4::lookAt(shot.position, shot.target, {0.0f, 1.0f, 0.0f});
    view.projection = Mat4::perspective(shot.fovRadians, aspect, shot.nearPlane, shot.farPlane);
    return view;
}

bool FixedCameraRig::empty() const {
    return shots_.empty();
}

const std::vector<FixedCameraShot>& FixedCameraRig::shots() const {
    return shots_;
}

} // namespace Exo
