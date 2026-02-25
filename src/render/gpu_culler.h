#pragma once

#include "gpu_buffer.h"
#include "vulkan/vulkan_compute_pipeline.h"
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

class VulkanShaderManager;

/**
 * CullObjectData — Per-object data for GPU frustum culling.
 * 
 * This is separate from ObjectData (render SSBO) because:
 * - Culling only needs bounds, not materials/textures
 * - Smaller struct = better GPU cache efficiency
 * - Can be updated independently of render data
 * 
 * Must match gpu_cull.comp CullObjectData struct (32 bytes).
 */
struct CullObjectData {
    float boundingSphere[4];  // xyz = center (world space), w = radius
    uint32_t objectIndex;     // Index into ObjectData SSBO for rendering
    uint32_t batchId;         // Which batch this object belongs to
    uint32_t _pad0;
    uint32_t _pad1;
};
static_assert(sizeof(CullObjectData) == 32, "CullObjectData must be 32 bytes");

/**
 * FrustumData — Camera frustum planes for GPU culling.
 * 
 * Must match gpu_cull.comp FrustumData struct (112 bytes).
 */
struct FrustumData {
    float planes[6][4];       // 6 planes: left, right, bottom, top, near, far (Ax + By + Cz + D)
    uint32_t objectCount;     // Total objects to cull
    uint32_t _pad0;
    uint32_t _pad1;
    uint32_t _pad2;
};
static_assert(sizeof(FrustumData) == 112, "FrustumData must be 112 bytes");

/**
 * VkDrawIndexedIndirectCommand — For reference (matches Vulkan spec).
 */
struct DrawIndexedIndirectCommand {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t  vertexOffset;
    uint32_t firstInstance;
};
static_assert(sizeof(DrawIndexedIndirectCommand) == 20, "DrawIndexedIndirectCommand must be 20 bytes");

/**
 * GPUCuller — GPU-driven frustum culling using compute shaders.
 * 
 * Architecture:
 *   CPU: Upload all object bounds to cull input buffer
 *   CPU: Upload frustum planes to uniform buffer
 *   CPU: Reset visible count to 0
 *   GPU: Dispatch compute shader (tests all objects in parallel)
 *   GPU: Compute shader atomically appends visible indices
 *   CPU: Pipeline barrier (compute → vertex/indirect)
 *   GPU: Draw using indirect commands
 * 
 * Buffers:
 *   - Frustum UBO: Camera frustum planes (updated per-frame)
 *   - Cull Input SSBO: All object bounds (updated when objects change)
 *   - Visible Indices SSBO: Output list of visible object indices
 *   - Atomic Counter SSBO: Number of visible objects
 *   - Indirect Commands SSBO: Draw commands with instance counts
 */
class GPUCuller {
public:
    GPUCuller() = default;
    ~GPUCuller();

    // Non-copyable, non-movable (owns Vulkan resources)
    GPUCuller(const GPUCuller&) = delete;
    GPUCuller& operator=(const GPUCuller&) = delete;

    /**
     * Create GPU culler resources.
     * 
     * @param device Vulkan logical device
     * @param physicalDevice Physical device (for memory allocation)
     * @param pShaderManager Shader manager for loading compute shader
     * @param maxObjects Maximum number of objects to cull
     * @param maxBatches Maximum number of draw batches (indirect commands)
     * @return true on success
     */
    bool Create(VkDevice device,
                VkPhysicalDevice physicalDevice,
                VulkanShaderManager* pShaderManager,
                uint32_t maxObjects,
                uint32_t maxBatches = 1);

    /**
     * Destroy all GPU resources.
     */
    void Destroy();

    /**
     * Update frustum planes for culling.
     * Call this each frame before Dispatch().
     * 
     * @param planes 6 frustum planes [left, right, bottom, top, near, far]
     *               Each plane is [A, B, C, D] where Ax + By + Cz + D = 0
     * @param objectCount Number of objects to cull (must <= maxObjects)
     */
    void UpdateFrustum(const float planes[6][4], uint32_t objectCount);

    /**
     * Upload object culling data.
     * Call this when objects are added/removed/transformed.
     * 
     * @param pObjects Array of CullObjectData
     * @param count Number of objects
     */
    void UploadCullObjects(const CullObjectData* pObjects, uint32_t count);

    /**
     * Reset visible count and indirect commands before dispatch.
     * Must be called before Dispatch() each frame.
     * 
     * @param pCmdBuffer Command buffer to record reset commands
     */
    void ResetCounters(VkCommandBuffer cmdBuffer);

    /**
     * Dispatch compute shader for GPU culling.
     * 
     * @param cmdBuffer Command buffer to record dispatch
     */
    void Dispatch(VkCommandBuffer cmdBuffer);

    /**
     * Insert pipeline barrier after dispatch (compute → draw).
     * 
     * @param cmdBuffer Command buffer
     */
    void BarrierAfterDispatch(VkCommandBuffer cmdBuffer);

    /**
     * Get indirect draw commands buffer for vkCmdDrawIndexedIndirect.
     */
    VkBuffer GetIndirectBuffer() const { return m_indirectBuffer.GetBuffer(); }

    /**
     * Get visible indices buffer (for vertex shader to read).
     */
    VkBuffer GetVisibleIndicesBuffer() const { return m_visibleIndicesBuffer.GetBuffer(); }

    /**
     * Get atomic counter buffer (for debug/stats readback).
     */
    VkBuffer GetAtomicCounterBuffer() const { return m_atomicCounterBuffer.GetBuffer(); }

    /**
     * Read back visible count (for CPU-side stats).
     * Only call after GPU has finished (fence wait).
     * 
     * @return Number of visible objects from last dispatch
     */
    uint32_t ReadbackVisibleCount();

    bool IsValid() const { return m_device != VK_NULL_HANDLE && m_computePipeline.IsValid(); }

private:
    bool CreateDescriptorSetLayout();
    bool CreateDescriptorPool();
    bool CreateDescriptorSet();

    VkDevice         m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    
    uint32_t m_maxObjects = 0;
    uint32_t m_maxBatches = 1;
    uint32_t m_currentObjectCount = 0;

    // Compute pipeline
    VulkanComputePipeline m_computePipeline;

    // Descriptor set layout and pool
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet       m_descriptorSet = VK_NULL_HANDLE;

    // GPU Buffers
    GPUBuffer m_frustumBuffer;        // Set 0, Binding 0: Frustum UBO
    GPUBuffer m_cullInputBuffer;      // Set 0, Binding 1: Cull objects SSBO
    GPUBuffer m_visibleIndicesBuffer; // Set 0, Binding 2: Visible indices SSBO (output)
    GPUBuffer m_atomicCounterBuffer;  // Set 0, Binding 3: Atomic counter SSBO
    GPUBuffer m_indirectBuffer;       // Set 0, Binding 4: Indirect commands SSBO
};
