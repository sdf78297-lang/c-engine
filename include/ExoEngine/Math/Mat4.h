#pragma once

#include <array>

#include <ExoEngine/Math/Vec.h>

namespace Exo {

class Mat4 {
public:
    std::array<float, 16> values {};

    static Mat4 identity();
    static Mat4 perspective(float fovRadians, float aspect, float nearPlane, float farPlane);
    static Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up);
    static Mat4 translate(Vec3 offset);

    [[nodiscard]] const float* data() const {
        return values.data();
    }
};

Mat4 operator*(const Mat4& lhs, const Mat4& rhs);

} // namespace Exo
