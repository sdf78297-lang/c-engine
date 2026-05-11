#include <ExoEngine/Math/Mat4.h>

#include <cmath>

namespace Exo {

Mat4 Mat4::identity() {
    Mat4 result;
    result.values[0] = 1.0f;
    result.values[5] = 1.0f;
    result.values[10] = 1.0f;
    result.values[15] = 1.0f;
    return result;
}

Mat4 Mat4::perspective(float fovRadians, float aspect, float nearPlane, float farPlane) {
    Mat4 result;
    const float tanHalfFov = std::tan(fovRadians * 0.5f);

    result.values[0] = 1.0f / (aspect * tanHalfFov);
    result.values[5] = 1.0f / tanHalfFov;
    result.values[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    result.values[11] = -1.0f;
    result.values[14] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);

    return result;
}

Mat4 Mat4::lookAt(Vec3 eye, Vec3 target, Vec3 up) {
    const Vec3 forward = normalize(target - eye);
    const Vec3 side = normalize(cross(forward, normalize(up)));
    const Vec3 cameraUp = cross(side, forward);

    Mat4 result = Mat4::identity();
    result.values[0] = side.x;
    result.values[4] = side.y;
    result.values[8] = side.z;
    result.values[1] = cameraUp.x;
    result.values[5] = cameraUp.y;
    result.values[9] = cameraUp.z;
    result.values[2] = -forward.x;
    result.values[6] = -forward.y;
    result.values[10] = -forward.z;
    result.values[12] = -dot(side, eye);
    result.values[13] = -dot(cameraUp, eye);
    result.values[14] = dot(forward, eye);
    return result;
}

Mat4 Mat4::translate(Vec3 offset) {
    Mat4 result = Mat4::identity();
    result.values[12] = offset.x;
    result.values[13] = offset.y;
    result.values[14] = offset.z;
    return result;
}

Mat4 Mat4::scale(Vec3 factors) {
    Mat4 result = Mat4::identity();
    result.values[0] = factors.x;
    result.values[5] = factors.y;
    result.values[10] = factors.z;
    return result;
}

Mat4 Mat4::rotateX(float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    Mat4 result = Mat4::identity();
    result.values[5] = c;
    result.values[6] = s;
    result.values[9] = -s;
    result.values[10] = c;
    return result;
}

Mat4 Mat4::rotateY(float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    Mat4 result = Mat4::identity();
    result.values[0] = c;
    result.values[2] = -s;
    result.values[8] = s;
    result.values[10] = c;
    return result;
}

Mat4 Mat4::rotateZ(float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    Mat4 result = Mat4::identity();
    result.values[0] = c;
    result.values[1] = s;
    result.values[4] = -s;
    result.values[5] = c;
    return result;
}

Mat4 operator*(const Mat4& lhs, const Mat4& rhs) {
    Mat4 result;

    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int i = 0; i < 4; ++i) {
                sum += lhs.values[i * 4 + row] * rhs.values[column * 4 + i];
            }
            result.values[column * 4 + row] = sum;
        }
    }

    return result;
}

} // namespace Exo
