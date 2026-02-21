/*
 * Camera — position, rotation, and view matrix.
 */
#include "camera.h"
#include <algorithm>

constexpr float kPitchLimit = 1.553f; // ~89 degrees in radians
constexpr float kPi = 3.14159265358979f;

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

void Camera::SetRotation(float yaw, float pitch) {
    m_yaw = yaw;
    m_pitch = std::clamp(pitch, -kPitchLimit, kPitchLimit);
}

void Camera::GetRotation(float& yaw, float& pitch) const {
    yaw = m_yaw;
    pitch = m_pitch;
}

void Camera::AddRotation(float deltaYaw, float deltaPitch) {
    m_yaw += deltaYaw;
    m_pitch = std::clamp(m_pitch + deltaPitch, -kPitchLimit, kPitchLimit);
    
    // Keep yaw in [-PI, PI] range
    while (m_yaw > kPi) m_yaw -= 2.f * kPi;
    while (m_yaw < -kPi) m_yaw += 2.f * kPi;
}

void Camera::GetForward(float& fx, float& fy, float& fz) const {
    // Forward = direction camera is looking (into screen = -Z when yaw=0, pitch=0)
    float cosPitch = std::cos(m_pitch);
    fx = std::sin(m_yaw) * cosPitch;
    fy = std::sin(m_pitch);
    fz = -std::cos(m_yaw) * cosPitch;
}

void Camera::GetRight(float& rx, float& ry, float& rz) const {
    // Right = perpendicular to forward in XZ plane
    rx = std::cos(m_yaw);
    ry = 0.f;
    rz = std::sin(m_yaw);
}

void Camera::GetViewMatrix(float* pOut16_out) const {
    // Standard lookAt-style view matrix (column-major for Vulkan/GLSL)
    float cosY = std::cos(m_yaw);
    float sinY = std::sin(m_yaw);
    float cosP = std::cos(m_pitch);
    float sinP = std::sin(m_pitch);
    
    // Forward direction (where camera looks)
    float fx = sinY * cosP;
    float fy = sinP;
    float fz = -cosY * cosP;
    
    // Right direction (perpendicular to forward in XZ plane)
    float rx = cosY;
    float ry = 0.f;
    float rz = sinY;
    
    // Up direction = cross(right, forward)
    float ux = -sinY * sinP;
    float uy = cosP;
    float uz = cosY * sinP;
    
    float px = m_position[0];
    float py = m_position[1];
    float pz = m_position[2];
    
    // View matrix rows: right, up, -forward
    // But stored column-major: m[i*4 + j] = row j, column i
    
    // Column 0 (x components of each row vector)
    pOut16_out[0]  = rx;
    pOut16_out[1]  = ux;
    pOut16_out[2]  = -fx;
    pOut16_out[3]  = 0.f;
    
    // Column 1 (y components)
    pOut16_out[4]  = ry;
    pOut16_out[5]  = uy;
    pOut16_out[6]  = -fy;
    pOut16_out[7]  = 0.f;
    
    // Column 2 (z components)
    pOut16_out[8]  = rz;
    pOut16_out[9]  = uz;
    pOut16_out[10] = -fz;
    pOut16_out[11] = 0.f;
    
    // Column 3 (translation: -dot of each row vector with position)
    pOut16_out[12] = -(rx * px + ry * py + rz * pz);
    pOut16_out[13] = -(ux * px + uy * py + uz * pz);
    pOut16_out[14] = -(-fx * px + -fy * py + -fz * pz);
    pOut16_out[15] = 1.f;
}

glm::mat4 Camera::GetViewMatrix() const {
    float viewMat[16];
    GetViewMatrix(viewMat);
    return glm::mat4(
        viewMat[0], viewMat[1], viewMat[2], viewMat[3],
        viewMat[4], viewMat[5], viewMat[6], viewMat[7],
        viewMat[8], viewMat[9], viewMat[10], viewMat[11],
        viewMat[12], viewMat[13], viewMat[14], viewMat[15]
    );
}
