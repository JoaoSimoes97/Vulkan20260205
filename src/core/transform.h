/*
 * Transform — Position, rotation, scale for GameObjects.
 * Always present on every GameObject. Stored in SoA pool for cache efficiency.
 */
#pragma once

#include <cmath>
#include <cstring>

/**
 * Transform component data.
 * Uses raw floats for compatibility with existing object.h math functions.
 * Can be used with glm if available.
 */
struct Transform {
    /** World position (x, y, z). */
    float position[3] = {0.f, 0.f, 0.f};

    /** Rotation quaternion (x, y, z, w). Identity = (0, 0, 0, 1). */
    float rotation[4] = {0.f, 0.f, 0.f, 1.f};

    /** Scale (x, y, z). Uniform scale = (1, 1, 1). */
    float scale[3] = {1.f, 1.f, 1.f};

    /** Dirty flag for caching model matrix. */
    bool bDirty = true;

    /** Cached model matrix (column-major 4x4). Recomputed when dirty. */
    float modelMatrix[16] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    };
};

/** Set transform position. Marks dirty. */
inline void TransformSetPosition(Transform& t, float x, float y, float z) {
    t.position[0] = x;
    t.position[1] = y;
    t.position[2] = z;
    t.bDirty = true;
}

/** Set transform rotation from quaternion (x, y, z, w). Marks dirty. */
inline void TransformSetRotation(Transform& t, float qx, float qy, float qz, float qw) {
    t.rotation[0] = qx;
    t.rotation[1] = qy;
    t.rotation[2] = qz;
    t.rotation[3] = qw;
    t.bDirty = true;
}

/** Set transform scale. Marks dirty. */
inline void TransformSetScale(Transform& t, float sx, float sy, float sz) {
    t.scale[0] = sx;
    t.scale[1] = sy;
    t.scale[2] = sz;
    t.bDirty = true;
}

/** Build model matrix from position, rotation (quaternion), scale. Result = T * R * S. Column-major. */
inline void TransformBuildModelMatrix(Transform& t) {
    if (!t.bDirty) return;

    float* m = t.modelMatrix;
    const float* p = t.position;
    const float* q = t.rotation;
    const float* s = t.scale;

    // Quaternion to rotation matrix
    float qx = q[0], qy = q[1], qz = q[2], qw = q[3];
    float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    float xy = qx * qy, xz = qx * qz, xw = qx * qw;
    float yz = qy * qz, yw = qy * qw, zw = qz * qw;

    // Rotation matrix (column-major) with scale applied
    m[0]  = (1.f - 2.f * (yy + zz)) * s[0];
    m[1]  = (2.f * (xy + zw)) * s[0];
    m[2]  = (2.f * (xz - yw)) * s[0];
    m[3]  = 0.f;

    m[4]  = (2.f * (xy - zw)) * s[1];
    m[5]  = (1.f - 2.f * (xx + zz)) * s[1];
    m[6]  = (2.f * (yz + xw)) * s[1];
    m[7]  = 0.f;

    m[8]  = (2.f * (xz + yw)) * s[2];
    m[9]  = (2.f * (yz - xw)) * s[2];
    m[10] = (1.f - 2.f * (xx + yy)) * s[2];
    m[11] = 0.f;

    // Translation
    m[12] = p[0];
    m[13] = p[1];
    m[14] = p[2];
    m[15] = 1.f;

    t.bDirty = false;
}

/** Get forward direction (-Z in local space) transformed by rotation. */
inline void TransformGetForward(const Transform& t, float& fx, float& fy, float& fz) {
    float qx = t.rotation[0], qy = t.rotation[1], qz = t.rotation[2], qw = t.rotation[3];
    // Rotate (0, 0, -1) by quaternion
    fx = -2.f * (qx * qz + qy * qw);
    fy = -2.f * (qy * qz - qx * qw);
    fz = -(1.f - 2.f * (qx * qx + qy * qy));
}

/** Get up direction (+Y in local space) transformed by rotation. */
inline void TransformGetUp(const Transform& t, float& ux, float& uy, float& uz) {
    float qx = t.rotation[0], qy = t.rotation[1], qz = t.rotation[2], qw = t.rotation[3];
    // Rotate (0, 1, 0) by quaternion
    ux = 2.f * (qx * qy - qz * qw);
    uy = 1.f - 2.f * (qx * qx + qz * qz);
    uz = 2.f * (qy * qz + qx * qw);
}

/** Get right direction (+X in local space) transformed by rotation. */
inline void TransformGetRight(const Transform& t, float& rx, float& ry, float& rz) {
    float qx = t.rotation[0], qy = t.rotation[1], qz = t.rotation[2], qw = t.rotation[3];
    // Rotate (1, 0, 0) by quaternion
    rx = 1.f - 2.f * (qy * qy + qz * qz);
    ry = 2.f * (qx * qy + qz * qw);
    rz = 2.f * (qx * qz - qy * qw);
}

/**
 * Decompose a column-major 4x4 TRS matrix into position, rotation (quaternion), scale.
 * Assumes M = T * R * S (translation * rotation * scale).
 * @param m Column-major 4x4 matrix.
 * @param t Output transform with position, rotation, scale.
 */
inline void TransformFromMatrix(const float* m, Transform& t) {
    // Position is in the last column
    t.position[0] = m[12];
    t.position[1] = m[13];
    t.position[2] = m[14];

    // Scale is the length of each column (columns 0, 1, 2)
    float sx = std::sqrt(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
    float sy = std::sqrt(m[4]*m[4] + m[5]*m[5] + m[6]*m[6]);
    float sz = std::sqrt(m[8]*m[8] + m[9]*m[9] + m[10]*m[10]);
    t.scale[0] = sx;
    t.scale[1] = sy;
    t.scale[2] = sz;

    // Extract rotation matrix by dividing out scale
    float r00 = m[0] / sx, r01 = m[4] / sy, r02 = m[8] / sz;
    float r10 = m[1] / sx, r11 = m[5] / sy, r12 = m[9] / sz;
    float r20 = m[2] / sx, r21 = m[6] / sy, r22 = m[10] / sz;

    // Convert rotation matrix to quaternion
    // Using Shepperd's method for numerical stability
    float trace = r00 + r11 + r22;
    float qx, qy, qz, qw;

    if (trace > 0.f) {
        float s = 0.5f / std::sqrt(trace + 1.f);
        qw = 0.25f / s;
        qx = (r21 - r12) * s;
        qy = (r02 - r20) * s;
        qz = (r10 - r01) * s;
    } else if (r00 > r11 && r00 > r22) {
        float s = 2.f * std::sqrt(1.f + r00 - r11 - r22);
        qw = (r21 - r12) / s;
        qx = 0.25f * s;
        qy = (r01 + r10) / s;
        qz = (r02 + r20) / s;
    } else if (r11 > r22) {
        float s = 2.f * std::sqrt(1.f + r11 - r00 - r22);
        qw = (r02 - r20) / s;
        qx = (r01 + r10) / s;
        qy = 0.25f * s;
        qz = (r12 + r21) / s;
    } else {
        float s = 2.f * std::sqrt(1.f + r22 - r00 - r11);
        qw = (r10 - r01) / s;
        qx = (r02 + r20) / s;
        qy = (r12 + r21) / s;
        qz = 0.25f * s;
    }

    // Normalize quaternion
    float len = std::sqrt(qx*qx + qy*qy + qz*qz + qw*qw);
    if (len > 0.0001f) {
        t.rotation[0] = qx / len;
        t.rotation[1] = qy / len;
        t.rotation[2] = qz / len;
        t.rotation[3] = qw / len;
    } else {
        t.rotation[0] = 0.f;
        t.rotation[1] = 0.f;
        t.rotation[2] = 0.f;
        t.rotation[3] = 1.f;
    }

    t.bDirty = true;
}

