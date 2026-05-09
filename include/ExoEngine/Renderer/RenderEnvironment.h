#pragma once

#include <ExoEngine/Math/Vec.h>

namespace Exo {

struct RenderEnvironment {
    Vec3 clearColor {0.014f, 0.017f, 0.020f};
    Vec3 ambientColor {0.45f, 0.52f, 0.62f};
    float ambientIntensity = 0.30f;
    Vec3 keyLightDirection {0.40f, 0.85f, 0.32f};
    Vec3 keyLightColor {0.82f, 0.90f, 1.0f};
    float keyLightIntensity = 0.42f;
    Vec3 fogColor {0.026f, 0.034f, 0.038f};
    float fogStart = 3.5f;
    float fogDensity = 0.065f;
    float exposure = 1.02f;
    float contrast = 1.10f;
    float saturation = 0.86f;
    float vignetteStrength = 0.48f;
};

} // namespace Exo
