#pragma once

#include <ExoEngine/Math/Vec.h>
#include <ExoEngine/Scene/FixedCameraRig.h>

namespace Exo {

struct PlayerInput {
    float forwardAxis = 0.0f;
    float turnAxis = 0.0f;
    bool run = false;
};

struct PlayerMotorConfig {
    float walkSpeed = 1.55f;
    float runMultiplier = 1.65f;
    float turnSpeedRadians = 2.45f;
    float eyeHeight = 1.65f;
};

class PlayerMotor {
public:
    explicit PlayerMotor(PlayerMotorConfig config = {});

    void update(Vec3& position, float& yaw, const PlayerInput& input, const Bounds3& walkBounds, float deltaSeconds) const;

private:
    PlayerMotorConfig config_;
};

} // namespace Exo
