#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

struct MaterialHandle;
class MeshHandle;
class TextureHandle;
struct Object;

/**
 * Instance tiers for GPU buffer management.
 * See docs/instancing-architecture.md for full design.
 */
enum class InstanceTier : uint8_t {
    Static      = 0,  // Never moves after level load, GPU-resident
    SemiStatic  = 1,  // Moves infrequently (dirty flag pattern)
    Dynamic     = 2,  // Moves every frame (ring-buffered)
    Procedural  = 3   // GPU-generated via compute shaders
};

/**
 * Parse instanceTier string from JSON.
 * @param tierStr String value: "static", "semi-static", "dynamic", "procedural"
 * @return Corresponding InstanceTier enum value (defaults to Static)
 */
inline InstanceTier ParseInstanceTier(const std::string& tierStr) {
    if (tierStr == "semi-static") return InstanceTier::SemiStatic;
    if (tierStr == "dynamic") return InstanceTier::Dynamic;
    if (tierStr == "procedural") return InstanceTier::Procedural;
    return InstanceTier::Static;  // Default
}

/** Per-object update callback. Called each frame with object reference and delta time in seconds. */
using ObjectUpdateCallback = std::function<void(Object&, float)>;

/**
 * Axis-aligned bounding box (local space).
 */
struct AABB {
    float minX = 0.f, minY = 0.f, minZ = 0.f;
    float maxX = 0.f, maxY = 0.f, maxZ = 0.f;
    
    /** Compute bounding sphere radius from AABB (half-diagonal). */
    float GetBoundingSphereRadius() const {
        float dx = maxX - minX;
        float dy = maxY - minY;
        float dz = maxZ - minZ;
        return 0.5f * std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    
    /** Get center of AABB. */
    void GetCenter(float& cx, float& cy, float& cz) const {
        cx = (minX + maxX) * 0.5f;
        cy = (minY + maxY) * 0.5f;
        cz = (minZ + maxZ) * 0.5f;
    }
};

/**
 * Bounding sphere for frustum culling (world space).
 */
struct BoundingSphere {
    float centerX = 0.f, centerY = 0.f, centerZ = 0.f;
    float radius = 0.f;  // 0 = not computed yet
};

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

// C4324: structure was padded due to alignment specifier - intentional for GPU data alignment
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)
#endif

struct Object {
    /** Material, mesh, and texture refs; draw list resolves these to VkPipeline, buffers, and descriptor sets. */
    std::shared_ptr<MaterialHandle> pMaterial;
    std::shared_ptr<MeshHandle>     pMesh;
    std::shared_ptr<TextureHandle>  pTexture;                  // Per-object texture (from glTF baseColorTexture); nullptr = use default white
    std::shared_ptr<TextureHandle>  pMetallicRoughnessTexture; // Metallic-roughness texture (glTF); nullptr = use factors only
    std::shared_ptr<TextureHandle>  pEmissiveTexture;          // Emissive texture (glTF); nullptr = use emissiveFactor only
    std::shared_ptr<TextureHandle>  pNormalTexture;            // Normal map texture (glTF); nullptr = use vertex normals only
    std::shared_ptr<TextureHandle>  pOcclusionTexture;         // Ambient occlusion texture (glTF); nullptr = no AO

    /** Render mode: visualization choice (solid vs wireframe). Can be overridden at runtime. */
    RenderMode               renderMode    = RenderMode::Auto;
    /** Local transform (column-major mat4). Used with projection to fill pushData each frame. */
    alignas(16) float        localTransform[16] = {
        1.f, 0.f, 0.f, 0.f,  0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,  0.f, 0.f, 0.f, 1.f
    };
    /** Per-object color (RGBA). From glTF baseColorFactor; passed to fragment shader via push constants. */
    float                    color[4]      = { 1.f, 1.f, 1.f, 1.f };
    /** Emissive color (RGB) + strength (A). For self-illuminated materials. */
    float                    emissive[4]   = { 0.f, 0.f, 0.f, 1.f };
    /** Whether this object emits light into the scene (creates a point light). */
    bool                     emitsLight    = false;
    /** Light radius for emissive objects (how far the light reaches). */
    float                    emissiveLightRadius = 15.f;
    /** Light intensity multiplier for emissive objects. */
    float                    emissiveLightIntensity = 5.f;
    /** 
     * Associated emissive light's GameObject ID in SceneNew.
     * UINT32_MAX means no light exists yet. Set by SceneManager::SyncEmissiveLights().
     * This creates a proper parent-child relationship: mesh object → light entity.
     */
    uint32_t                 emissiveLightId = UINT32_MAX;
    /** Metallic factor (0-1). From glTF pbrMetallicRoughness.metallicFactor. */
    float                    metallicFactor  = 1.f;
    /** Roughness factor (0-1). From glTF pbrMetallicRoughness.roughnessFactor. */
    float                    roughnessFactor = 1.f;
    /** Normal texture scale. From glTF normalTexture.scale (default 1.0). */
    float                    normalScale = 1.f;
    /** Occlusion texture strength. From glTF occlusionTexture.strength (default 1.0). */
    float                    occlusionStrength = 1.f;
    /** Local-space AABB (computed from mesh vertices). Used for bounding sphere calculation. */
    AABB                     localAABB;
    /** World-space bounding sphere (computed each frame from localAABB + transform). Used for frustum culling. */
    BoundingSphere           worldBounds;
    /** Optional per-object update callback. If set, called each frame with deltaTime before rendering. */
    ObjectUpdateCallback     onUpdate;
    /** Arbitrary data pushed to the GPU (e.g. mat4 + color). Filled each frame from projection * localTransform + color. */
    std::vector<uint8_t>     pushData;
    uint32_t                 pushDataSize  = 0u;
    /** Optional name for editor display. */
    std::string              name;
    /** Link to corresponding GameObject in SceneNew. UINT32_MAX = no link. */
    uint32_t                 gameObjectId  = UINT32_MAX;
    /** Instance tier for GPU buffer management. Determines update frequency and culling strategy. */
    InstanceTier             instanceTier  = InstanceTier::Static;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

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

/** Push layout: mat4 (64 bytes) + vec4 color (16 bytes) + uint objectIndex (4 bytes) + padding (12 bytes) + vec4 camPos (16 bytes) = 112 bytes.
    NOTE: vec4 requires 16-byte alignment in GLSL, so camPos must start at offset 96 (multiple of 16). 
    DEPRECATED: Use kInstancedPushConstantSize for instanced rendering. */
constexpr uint32_t kObjectMat4Bytes       = 64u;
constexpr uint32_t kObjectColorBytes      = 16u;
constexpr uint32_t kObjectIndexBytes      = 4u;
constexpr uint32_t kObjectPushPaddingBytes = 12u;  // Align camPos to 16 bytes (offset 96)
constexpr uint32_t kObjectCamPosBytes     = 16u;
constexpr uint32_t kObjectPushConstantSize = kObjectMat4Bytes + kObjectColorBytes + kObjectIndexBytes + kObjectPushPaddingBytes + kObjectCamPosBytes;

// Push constant offset validations (MUST match GLSL layout) - DEPRECATED
constexpr uint32_t kPushOffset_MVP        = 0u;                                                         // mat4 at offset 0
constexpr uint32_t kPushOffset_Color      = kObjectMat4Bytes;                                           // vec4 at offset 64
constexpr uint32_t kPushOffset_ObjectIdx  = kPushOffset_Color + kObjectColorBytes;                      // uint at offset 80
constexpr uint32_t kPushOffset_Padding    = kPushOffset_ObjectIdx + kObjectIndexBytes;                  // padding at offset 84
constexpr uint32_t kPushOffset_CamPos     = kPushOffset_Padding + kObjectPushPaddingBytes;              // vec4 at offset 96

// Static validations for layout correctness
static_assert(kPushOffset_MVP == 0, "MVP must be at offset 0");
static_assert(kPushOffset_Color == 64, "Color must be at offset 64");
static_assert(kPushOffset_ObjectIdx == 80, "ObjectIndex must be at offset 80");
static_assert(kPushOffset_CamPos == 96, "CamPos must be at offset 96 (16-byte aligned for vec4)");
static_assert(kPushOffset_CamPos % 16 == 0, "CamPos offset must be 16-byte aligned for GLSL vec4");
static_assert(kObjectPushConstantSize == 112, "Total push constant size must be 112 bytes");

/**
 * Instanced Push Constant Layout (96 bytes):
 * - mat4 viewProj (64 bytes) at offset 0
 * - vec4 camPos (16 bytes) at offset 64
 * - uint batchStartIndex (4 bytes) at offset 80
 * - padding (12 bytes) at offset 84
 * Total: 96 bytes
 *
 * Objects are indexed via: batchStartIndex + gl_InstanceIndex
 */
constexpr uint32_t kInstancedViewProjBytes     = 64u;
constexpr uint32_t kInstancedCamPosBytes       = 16u;
constexpr uint32_t kInstancedBatchIndexBytes   = 4u;
constexpr uint32_t kInstancedPaddingBytes      = 12u;
constexpr uint32_t kInstancedPushConstantSize  = kInstancedViewProjBytes + kInstancedCamPosBytes + kInstancedBatchIndexBytes + kInstancedPaddingBytes;

// Instanced push constant offset validations (MUST match GLSL layout)
constexpr uint32_t kInstPushOffset_ViewProj      = 0u;                                                    // mat4 at offset 0
constexpr uint32_t kInstPushOffset_CamPos        = kInstancedViewProjBytes;                               // vec4 at offset 64
constexpr uint32_t kInstPushOffset_BatchIndex    = kInstPushOffset_CamPos + kInstancedCamPosBytes;        // uint at offset 80
constexpr uint32_t kInstPushOffset_Padding       = kInstPushOffset_BatchIndex + kInstancedBatchIndexBytes; // padding at offset 84

// Static validations for instanced layout correctness
static_assert(kInstPushOffset_ViewProj == 0, "ViewProj must be at offset 0");
static_assert(kInstPushOffset_CamPos == 64, "CamPos must be at offset 64");
static_assert(kInstPushOffset_BatchIndex == 80, "BatchIndex must be at offset 80");
static_assert(kInstancedPushConstantSize == 96, "Instanced push constant size must be 96 bytes");

/** DEPRECATED: Use ObjectFillInstancedPushData for instanced rendering.
    Fill object pushData from viewProj * localTransform, color, objectIndex, and camera position. Ensures pushData is sized; call each frame before draw. */
inline void ObjectFillPushData(Object& obj, const float* viewProj, uint32_t objectIndex, const float* camPos) {
    // Always ensure pushData is the correct size and pushDataSize is set
    if (obj.pushData.size() != kObjectPushConstantSize) {
        obj.pushData.resize(kObjectPushConstantSize);
    }
    obj.pushDataSize = kObjectPushConstantSize;  // Always set this
    
    if (viewProj) {
        ObjectMat4Multiply(reinterpret_cast<float*>(obj.pushData.data()), viewProj, obj.localTransform);
        std::memcpy(obj.pushData.data() + kObjectMat4Bytes, obj.color, kObjectColorBytes);
        std::memcpy(obj.pushData.data() + kObjectMat4Bytes + kObjectColorBytes, &objectIndex, kObjectIndexBytes);
        std::memset(obj.pushData.data() + kObjectMat4Bytes + kObjectColorBytes + kObjectIndexBytes, 0, kObjectPushPaddingBytes);
        // Camera position (vec4: xyz position, w unused)
        if (camPos) {
            std::memcpy(obj.pushData.data() + kObjectMat4Bytes + kObjectColorBytes + kObjectIndexBytes + kObjectPushPaddingBytes, camPos, 12);
            float w = 1.0f;
            std::memcpy(obj.pushData.data() + kObjectMat4Bytes + kObjectColorBytes + kObjectIndexBytes + kObjectPushPaddingBytes + 12, &w, 4);
        }
    }
}

/** Fill instanced push data (96 bytes) for GPU instanced rendering.
    Layout: viewProj (64) + camPos (16) + batchStartIndex (4) + padding (12)
    The shader computes MVP as viewProj * model, where model is fetched from SSBO using batchStartIndex + gl_InstanceIndex. */
inline void ObjectFillInstancedPushData(uint8_t* pOutData, const float* viewProj, const float* camPos, uint32_t batchStartIndex) {
    // viewProj at offset 0 (64 bytes)
    std::memcpy(pOutData, viewProj, 64);
    
    // camPos at offset 64 (16 bytes: xyz + w=1)
    if (camPos) {
        std::memcpy(pOutData + 64, camPos, 12);
        float w = 1.0f;
        std::memcpy(pOutData + 76, &w, 4);
    } else {
        std::memset(pOutData + 64, 0, 16);
    }
    
    // batchStartIndex at offset 80 (4 bytes)
    std::memcpy(pOutData + 80, &batchStartIndex, 4);
    
    // padding at offset 84 (12 bytes)
    std::memset(pOutData + 84, 0, 12);
}

