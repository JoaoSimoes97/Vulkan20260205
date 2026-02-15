#pragma once

/**
 * Camera: world-space position and view matrix (translate by -position).
 * Column-major mat4; +X right, +Y up, +Z out of screen.
 */
class Camera {
public:
    void SetPosition(float fX_ic, float fY_ic, float fZ_ic);
    void GetPosition(float& fX_out, float& fY_out, float& fZ_out) const;
    /** Write view matrix (translate by -position) into out16 (column-major, 16 floats). */
    void GetViewMatrix(float* pOut16_out) const;

private:
    float m_position[3] = { 0.f, 0.f, 0.f };
};
