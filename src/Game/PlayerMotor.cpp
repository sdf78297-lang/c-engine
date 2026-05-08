#include <ExoEngine/Game/PlayerMotor.h>

#include <algorithm>
#include <cmath>

namespace Exo {

namespace {

float clamp(float value, float minValue, float maxValue) {
    return std::max(minValue, std::min(value, maxValue));
}

} // namespace

PlayerMotor::PlayerMotor(PlayerMotorConfig config)
    : config_(config) {}

void PlayerMotor::update(
    Vec3& position,
    float& yaw,
    const PlayerInput& input,
    const Bounds3& walkBounds,
    float deltaSeconds) const {
    yaw += input.turnAxis * config_.turnSpeedRadians * deltaSeconds;

    if (std::abs(input.forwardAxis) > 0.001f) {
        const float speed = config_.walkSpeed * (input.run ? config_.runMultiplier : 1.0f);
        const Vec3 forward {std::sin(yaw), 0.0f, -std::cos(yaw)};
        position = position + (forward * (input.forwardAxis * speed * deltaSeconds));
    }

    position.x = clamp(position.x, walkBounds.min.x, walkBounds.max.x);
    position.z = clamp(position.z, walkBounds.min.z, walkBounds.max.z);
    position.y = config_.eyeHeight;
}

} // namespace Exo
