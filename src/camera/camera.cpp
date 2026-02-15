/*
 * Camera — position and view matrix.
 */
#include "camera.h"

void Camera::SetPosition(float fX_ic, float fY_ic, float fZ_ic) {
    this->m_position[0] = fX_ic;
    this->m_position[1] = fY_ic;
    this->m_position[2] = fZ_ic;
}

void Camera::GetPosition(float& fX_out, float& fY_out, float& fZ_out) const {
    fX_out = this->m_position[0];
    fY_out = this->m_position[1];
    fZ_out = this->m_position[2];
}

void Camera::GetViewMatrix(float* pOut16_out) const {
    for (int i = 0; i < 16; ++i)
        pOut16_out[i] = (i % 5 == 0) ? 1.f : 0.f;
    pOut16_out[12] = -this->m_position[0];
    pOut16_out[13] = -this->m_position[1];
    pOut16_out[14] = -this->m_position[2];
}
