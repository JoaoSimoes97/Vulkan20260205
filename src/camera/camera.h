#pragma once

#include <cmath>

/**
 * Camera: world-space position, yaw/pitch rotation, and view matrix.
 * Column-major mat4; +X right, +Y up, -Z forward (looking into screen).
 * FPS-style: yaw rotates around Y, pitch rotates around local X.
 */
class Camera {
public:
    void SetPosition(float fX_ic, float fY_ic, float fZ_ic);
    void GetPosition(float& fX_out, float& fY_out, float& fZ_out) const;
    
    /** Set yaw (horizontal) and pitch (vertical) in radians. */
    void SetRotation(float yaw, float pitch);
    void GetRotation(float& yaw, float& pitch) const;
    
    /** Add to yaw/pitch (for mouse look). Pitch is clamped to avoid gimbal lock. */
    void AddRotation(float deltaYaw, float deltaPitch);
    
    /** Get forward direction vector (normalized). */
    void GetForward(float& fx, float& fy, float& fz) const;
    
    /** Get right direction vector (normalized). */
    void GetRight(float& rx, float& ry, float& rz) const;
    
    /** Write view matrix into out16 (column-major, 16 floats). */
    void GetViewMatrix(float* pOut16_out) const;

private:
    float m_position[3] = { 0.f, 0.f, 0.f };
    float m_yaw = 0.f;    // Rotation around Y axis (radians), 0 = looking toward -Z
    float m_pitch = 0.f;  // Rotation around X axis (radians), clamped to [-89°, +89°]
};
