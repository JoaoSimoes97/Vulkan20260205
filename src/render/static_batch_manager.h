#pragma once
/**
 * @file static_batch_manager.h
 * @brief Manages Tier 0 (Static) and Tier 1 (Semi-Static) instances
 * 
 * GPU-resident instance data with optional dirty tracking for partial updates.
 * Designed for GPU culling via compute shader.
 * 
 * See docs/instancing-architecture.md for design details.
 */

#include "instance_types.h"
#include "gpu_buffer.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>

namespace render {

/**
 * StaticBatchManager — Manages GPU-resident static and semi-static instances.
 * 
 * Tier 0 (Static): Uploaded once at load time, never updated.
 * Tier 1 (Semi-Static): Uploaded at load, updated via dirty flag when transforms change.
 * 
 * Both tiers are culled by GPU compute shader.
 */
class StaticBatchManager {
public:
    StaticBatchManager() = default;
    ~StaticBatchManager();

    // Non-copyable
    StaticBatchManager(const StaticBatchManager&) = delete;
    StaticBatchManager& operator=(const StaticBatchManager&) = delete;

    /**
     * Initialize the manager with device and max capacity.
     * @param device Vulkan device
     * @param physicalDevice Physical device
     * @param maxInstances Maximum number of static/semi-static instances
     * @return true on success
     */
    bool Create(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t maxInstances);

    /**
     * Destroy all GPU resources.
     */
    void Destroy();

    /**
     * Add an instance. Returns instance ID, or UINT32_MAX on failure.
     * @param tier Must be Static or SemiStatic
     * @param transform World transform matrix
     * @param meshIndex Index into mesh table
     * @param materialIndex Index into material table
     * @param boundingSphere xyz=center (object space), w=radius
     * @return Instance ID for later reference
     */
    uint32_t AddInstance(InstanceTier tier,
                         const glm::mat4& transform,
                         uint32_t meshIndex,
                         uint32_t materialIndex,
                         const glm::vec4& boundingSphere);

    /**
     * Update an existing instance's transform. Only valid for SemiStatic tier.
     * Marks the instance as dirty for next FlushDirty() call.
     * @param instanceId ID returned from AddInstance
     * @param newTransform New world transform
     */
    void UpdateTransform(uint32_t instanceId, const glm::mat4& newTransform);

    /**
     * Mark an instance as dirty (for external modification tracking).
     * @param instanceId ID to mark dirty
     */
    void MarkDirty(uint32_t instanceId);

    /**
     * Upload all instance data to GPU.
     * Called once after all AddInstance() calls during level load.
     * @return true on success
     */
    bool UploadToGPU();

    /**
     * Flush dirty instances to GPU (partial buffer update).
     * Call once per frame if any transforms changed.
     * @return Number of instances updated
     */
    uint32_t FlushDirty();

    /**
     * Clear all instances (before loading new level).
     */
    void Clear();

    // Accessors
    VkBuffer GetInstanceBuffer() const { return m_instanceBuffer.GetBuffer(); }
    VkBuffer GetCullDataBuffer() const { return m_cullDataBuffer.GetBuffer(); }
    uint32_t GetInstanceCount() const { return static_cast<uint32_t>(m_instances.size()); }
    bool HasDirty() const { return !m_dirtySet.empty(); }

    /**
     * Get batches grouped by mesh/material for indirect drawing.
     */
    const std::unordered_map<BatchKey, std::vector<uint32_t>>& GetBatches() const { return m_batches; }

private:
    struct InstanceEntry {
        InstanceTier tier;
        GPUInstanceData instanceData;
        GPUCullData cullData;
    };

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    uint32_t m_maxInstances = 0;

    // CPU-side data
    std::vector<InstanceEntry> m_instances;
    std::unordered_set<uint32_t> m_dirtySet;
    std::unordered_map<BatchKey, std::vector<uint32_t>> m_batches;

    // GPU buffers
    GPUBuffer m_instanceBuffer;  // GPUInstanceData[] - transforms
    GPUBuffer m_cullDataBuffer;  // GPUCullData[] - culling input

    bool m_uploaded = false;
};

} // namespace render
