#pragma once

/**
 * Camera: world-space position and view matrix (translate by -position).
 * Column-major mat4; +X right, +Y up, +Z out of screen.
 */
class Camera {
public:
    void SetPosition(float x, float y, float z);
    void GetPosition(float& x, float& y, float& z) const;
    /** Write view matrix (translate by -position) into out16 (column-major, 16 floats). */
    void GetViewMatrix(float* out16) const;

private:
    float m_position[3] = { 0.f, 0.f, 0.f };
};
