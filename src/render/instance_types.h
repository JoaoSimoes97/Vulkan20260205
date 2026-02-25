#pragma once
/**
 * @file instance_types.h
 * @brief Multi-Tier Instance Rendering type definitions
 * 
 * Defines the tiered instancing system for GPU-optimized rendering:
 * - Tier 0 (Static): Never moves, GPU-culled
 * - Tier 1 (Semi-Static): Dirty flag updates, GPU-culled
 * - Tier 2 (Dynamic): Per-frame updates, CPU-culled (ring-buffered)
 * - Tier 3 (Procedural): Compute-generated instances
 * 
 * See docs/instancing-architecture.md for full design.
 */

#include <glm/glm.hpp>
#include <cstdint>

namespace render {

/**
 * Instance tier classification.
 * Determines how instance data is managed and culled.
 */
enum class InstanceTier : uint8_t {
    Static      = 0,  // Never moves after load (terrain, buildings)
    SemiStatic  = 1,  // Moves infrequently, dirty flag (doors, trees)
    Dynamic     = 2,  // Moves every frame (player, NPCs, physics)
    Procedural  = 3,  // Compute-generated (particles, grass)
    
    Count       = 4
};

/**
 * GPU instance transform data (64 bytes).
 * Stored in GPU-resident SSBO for static/semi-static tiers.
 */
struct alignas(16) GPUInstanceData {
    glm::mat4 model;  // 64 bytes - world transform
    
    static_assert(sizeof(glm::mat4) == 64, "mat4 must be 64 bytes");
};
static_assert(sizeof(GPUInstanceData) == 64, "GPUInstanceData must be 64 bytes");

/**
 * GPU material properties (64 bytes).
 * Indexed by materialIndex in GPUCullData.
 */
struct alignas(16) GPUMaterialData {
    glm::vec4 baseColor;      // 16 bytes - RGBA
    glm::vec4 emissive;       // 16 bytes - RGB + strength
    glm::vec4 matProps;       // 16 bytes - metallic, roughness, normalScale, occlusion
    uint32_t  textureIndices; // 4 bytes - packed: base(8)|normal(8)|mr(8)|emissive(8)
    uint32_t  flags;          // 4 bytes - material flags (doubleSided, alphaMode, etc.)
    float     _pad[2];        // 8 bytes - padding
    // Total: 64 bytes
};
static_assert(sizeof(GPUMaterialData) == 64, "GPUMaterialData must be 64 bytes");

/**
 * GPU cull data per instance (32 bytes).
 * Input to compute culling shader.
 */
struct alignas(16) GPUCullData {
    glm::vec4 boundingSphere;  // 16 bytes - xyz=center (object space), w=radius
    uint32_t  meshIndex;       // 4 bytes - index into mesh table for indirect draw
    uint32_t  materialIndex;   // 4 bytes - index into GPUMaterialData SSBO
    uint32_t  instanceIndex;   // 4 bytes - index into GPUInstanceData SSBO
    uint32_t  _pad;            // 4 bytes - padding
    // Total: 32 bytes
};
static_assert(sizeof(GPUCullData) == 32, "GPUCullData must be 32 bytes");

/**
 * Indirect draw command (matches VkDrawIndexedIndirectCommand).
 * Written by compute culling shader.
 */
struct GPUDrawIndirectCommand {
    uint32_t indexCount;
    uint32_t instanceCount;    // Filled by compute shader
    uint32_t firstIndex;
    int32_t  vertexOffset;
    uint32_t firstInstance;    // Start offset into visible instance list
};
static_assert(sizeof(GPUDrawIndirectCommand) == 20, "GPUDrawIndirectCommand must be 20 bytes");

/**
 * Mesh draw info for indirect drawing.
 * Stored per unique mesh.
 */
struct MeshDrawInfo {
    uint32_t indexCount;
    uint32_t firstIndex;
    int32_t  vertexOffset;
    uint32_t meshId;           // For debug/identification
};

/**
 * Instance registration descriptor.
 * Used when adding instances to the render system.
 */
struct InstanceDesc {
    InstanceTier tier;
    uint32_t     meshIndex;
    uint32_t     materialIndex;
    glm::mat4    transform;
    glm::vec4    boundingSphere;  // xyz=center (object space), w=radius
};

/**
 * Batch key for grouping instances.
 * Instances with same batch key can be drawn together.
 */
struct BatchKey {
    uint32_t meshIndex;
    uint32_t materialIndex;
    
    bool operator==(const BatchKey& other) const {
        return meshIndex == other.meshIndex && materialIndex == other.materialIndex;
    }
};

} // namespace render

// Hash for BatchKey (for use in unordered_map)
namespace std {
template<>
struct hash<render::BatchKey> {
    size_t operator()(const render::BatchKey& k) const {
        return hash<uint64_t>()(
            (static_cast<uint64_t>(k.meshIndex) << 32) | k.materialIndex
        );
    }
};
} // namespace std
