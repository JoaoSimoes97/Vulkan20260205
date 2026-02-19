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

