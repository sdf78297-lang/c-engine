#pragma once

#include <ExoEngine/Math/Vec.h>

namespace Exo {

struct RenderMaterialOverride {
    Vec3 colorTint {1.0f, 1.0f, 1.0f};
    Vec3 emissiveColor {0.0f, 0.0f, 0.0f};
    float emissiveIntensity = 0.0f;
};

} // namespace Exo
