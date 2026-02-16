#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

struct MaterialHandle;
class MeshHandle;
class TextureHandle;

/**
 * General drawable object: owns refs to material, mesh, and texture; per-object data (transform, color).
 * Everything from glTF: geometry (pMesh), appearance (pMaterial, pTexture, color from baseColorFactor).
 * RenderMode (solid vs wireframe) can be changed at runtime (editor, debug).
 */
enum class RenderMode {
    Auto,       // Use material properties (alphaMode) to determine pipeline
    Solid,      // Force solid/filled rendering
    Wireframe,  // Force wireframe rendering
};

struct Object {
    /** Material, mesh, and texture refs; draw list resolves these to VkPipeline, buffers, and descriptor sets. */
    std::shared_ptr<MaterialHandle> pMaterial;
    std::shared_ptr<MeshHandle>     pMesh;
    std::shared_ptr<TextureHandle>  pTexture; // Per-object texture (from glTF baseColorTexture); nullptr = use default white

    /** Render mode: visualization choice (solid vs wireframe). Can be overridden at runtime. */
    RenderMode               renderMode    = RenderMode::Auto;
    /** Local transform (column-major mat4). Used with projection to fill pushData each frame. */
    alignas(16) float        localTransform[16] = {
        1.f, 0.f, 0.f, 0.f,  0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,  0.f, 0.f, 0.f, 1.f
    };
    /** Per-object color (RGBA). From glTF baseColorFactor; passed to fragment shader via push constants. */
    float                    color[4]      = { 1.f, 1.f, 1.f, 1.f };
    /** Emissive color (RGB) + strength (A). For self-illuminated materials (future lighting). */
    float                    emissive[4]   = { 0.f, 0.f, 0.f, 1.f };
    /** Arbitrary data pushed to the GPU (e.g. mat4 + color). Filled each frame from projection * localTransform + color. */
    std::vector<uint8_t>     pushData;
    uint32_t                 pushDataSize  = 0u;
};

/** Identity matrix in column-major order (16 floats). */
inline void ObjectSetIdentity(float* out16) {
    for (int i = 0; i < 16; ++i) out16[i] = (i % 5 == 0) ? 1.f : 0.f;
}

/** Translation (tx, ty, tz) into column-major mat4. */
inline void ObjectSetTranslation(float* out16, float tx, float ty, float tz) {
    ObjectSetIdentity(out16);
    out16[12] = tx;
    out16[13] = ty;
    out16[14] = tz;
}

/** View matrix: translate(-x, -y, -z) for camera at (x, y, z). Column-major. World: +X right, +Y up, +Z out of screen; camera at (0,0,8) looks toward -Z. */
inline void ObjectSetViewTranslation(float* out16, float x, float y, float z) {
    ObjectSetIdentity(out16);
    out16[12] = -x;
    out16[13] = -y;
    out16[14] = -z;
}

/**
 * Orthographic projection for Vulkan (NDC depth 0..1, Y down). Column-major.
 * View space: Y up (bottom < top). NDC: Y down (top of screen = -1). So we flip Y.
 * Maps [left,right] x [bottom,top] x [nearZ,farZ] to NDC [-1,1] x [1,-1] x [0,1] (Y flipped).
 */
inline void ObjectSetOrtho(float* out16, float left, float right, float bottom, float top, float nearZ, float farZ) {
    for (int i = 0; i < 16; ++i) out16[i] = 0.f;
    out16[0]  = 2.f / (right - left);
    out16[5]  = -2.f / (top - bottom);  /* flip Y for Vulkan NDC (Y down) */
    out16[10] = 1.f / (farZ - nearZ);
    out16[12] = -(right + left) / (right - left);
    out16[13] = (top + bottom) / (top - bottom);  /* match flipped Y */
    out16[14] = -nearZ / (farZ - nearZ);
    out16[15] = 1.f;
}

/**
 * Perspective projection for Vulkan (NDC depth 0..1, Y down). Column-major.
 * View space: Y up. NDC: Y down (top = -1). So Y scale is negative (view +Y -> NDC -Y).
 * aspect = width/height. X scale = t/aspect so narrow window (small aspect) shows less horizontal FOV -> correct proportions.
 */
inline void ObjectSetPerspective(float* out16, float fovY_rad, float aspect, float nearZ, float farZ) {
    for (int i = 0; i < 16; ++i) out16[i] = 0.f;
    float t = 1.f / std::tan(fovY_rad * 0.5f);
    out16[0]  = t / aspect;  /* narrow window (small aspect) -> larger x scale -> less horizontal view -> no stretch */
    out16[5]  = -t;  /* view +Y -> NDC -Y (top of screen) */
    out16[10] = -farZ / (farZ - nearZ);
    out16[11] = -1.f;
    out16[14] = -nearZ * farZ / (farZ - nearZ);
}

/** Column-major mat4 multiply: out = A * B. out16 must have 16 floats. */
inline void ObjectMat4Multiply(float* out16, const float* proj16, const float* model16) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float v = 0.f;
            for (int k = 0; k < 4; ++k)
                v += proj16[row + k * 4] * model16[k + col * 4];
            out16[row + col * 4] = v;
        }
    }
}

/** Build column-major mat4 from position (xyz), rotation quaternion (xyzw), scale (xyz). Result = T * R * S. */
inline void ObjectSetFromPositionRotationScale(float* out16,
    float px, float py, float pz,
    float qx, float qy, float qz, float qw,
    float sx, float sy, float sz) {
    float r[16], s[16], t[16];
    ObjectSetIdentity(s);
    s[0] = sx; s[5] = sy; s[10] = sz;
    float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    float xy = qx * qy, xz = qx * qz, xw = qx * qw;
    float yz = qy * qz, yw = qy * qw, zw = qz * qw;
    r[0] = 1.f - 2.f * (yy + zz); r[4] = 2.f * (xy - zw);     r[8]  = 2.f * (xz + yw);     r[12] = 0.f;
    r[1] = 2.f * (xy + zw);     r[5] = 1.f - 2.f * (xx + zz); r[9]  = 2.f * (yz - xw);     r[13] = 0.f;
    r[2] = 2.f * (xz - yw);     r[6] = 2.f * (yz + xw);     r[10] = 1.f - 2.f * (xx + yy); r[14] = 0.f;
    r[3] = 0.f;                 r[7] = 0.f;                 r[11] = 0.f;                 r[15] = 1.f;
    ObjectSetIdentity(t);
    t[12] = px; t[13] = py; t[14] = pz;
    float rs[16];
    ObjectMat4Multiply(rs, r, s);
    ObjectMat4Multiply(out16, t, rs);
}

/** Push layout: mat4 (64 bytes) + vec4 color (16 bytes) = 80 bytes. */
constexpr uint32_t kObjectMat4Bytes       = 64u;
constexpr uint32_t kObjectColorBytes      = 16u;
constexpr uint32_t kObjectPushConstantSize = kObjectMat4Bytes + kObjectColorBytes;

/** Fill object pushData from viewProj * localTransform and color. Ensures pushData is sized; call each frame before draw. */
inline void ObjectFillPushData(Object& obj, const float* viewProj) {
    if (obj.pushData.size() < kObjectPushConstantSize) {
        obj.pushData.resize(kObjectPushConstantSize);
        obj.pushDataSize = kObjectPushConstantSize;
    }
    if (viewProj && obj.pushData.size() >= kObjectPushConstantSize) {
        ObjectMat4Multiply(reinterpret_cast<float*>(obj.pushData.data()), viewProj, obj.localTransform);
        std::memcpy(obj.pushData.data() + kObjectMat4Bytes, obj.color, kObjectColorBytes);
    }
}

