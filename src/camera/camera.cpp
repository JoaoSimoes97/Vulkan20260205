/*
 * Camera — position and view matrix.
 */
#include "camera.h"

void Camera::SetPosition(float x, float y, float z) {
    m_position[0] = x;
    m_position[1] = y;
    m_position[2] = z;
}

void Camera::GetPosition(float& x, float& y, float& z) const {
    x = m_position[0];
    y = m_position[1];
    z = m_position[2];
}

void Camera::GetViewMatrix(float* out16) const {
    for (int i = 0; i < 16; ++i)
        out16[i] = (i % 5 == 0) ? 1.f : 0.f;
    out16[12] = -m_position[0];
    out16[13] = -m_position[1];
    out16[14] = -m_position[2];
}
