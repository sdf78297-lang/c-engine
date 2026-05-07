#include <ExoEngine/Assets/MeshAsset.h>

#include <algorithm>

namespace Exo {

void MeshBounds::include(Vec3 point) {
    if (!valid) {
        min = point;
        max = point;
        valid = true;
        return;
    }

    min.x = std::min(min.x, point.x);
    min.y = std::min(min.y, point.y);
    min.z = std::min(min.z, point.z);
    max.x = std::max(max.x, point.x);
    max.y = std::max(max.y, point.y);
    max.z = std::max(max.z, point.z);
}

bool MeshAsset::empty() const {
    return vertices.empty() || indices.empty();
}

} // namespace Exo
