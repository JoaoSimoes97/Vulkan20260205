#pragma once
/**
 * @file render_system.h
 * @brief Unified multi-tier rendering system
 * 
 * Coordinates StaticBatchManager (Tier 0/1), dynamic instances (Tier 2),
 * and GPUCuller for efficient GPU-driven rendering.
 * 
 * See docs/instancing-architecture.md for design details.
 */

#include "instance_types.h"
#include "static_batch_manager.h"
#include "gpu_culler.h"
#include "gpu_buffer.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace render {

/**
 * Configuration for RenderSystem initialization.
 */
struct RenderSystemConfig {
    uint32_t maxStaticInstances = 4096;    // Tier 0 + 1
    uint32_t maxDynamicInstances = 1024;   // Tier 2
    uint32_t maxMeshes = 256;              // Unique meshes for indirect draw
    uint32_t framesInFlight = 2;           // Ring buffer frames
};

/**
 * RenderSystem — Unified interface for multi-tier instanced rendering.
 * 
 * Manages:
 * - Static instances (Tier 0): GPU-resident, GPU-culled
 * - Semi-static instances (Tier 1): GPU-resident with dirty updates, GPU-culled
 * - Dynamic instances (Tier 2): Ring-buffered, CPU-culled
 * 
 * Usage per frame:
 *   1. BeginFrame(frameIndex)
 *   2. UpdateCamera(viewProj)
 *   3. UpdateDynamicInstances() - populate Tier 2 data
 *   4. DispatchGPUCulling(cmd) - cull Tier 0/1 on GPU
 *   5. DrawStaticInstances(cmd) - indirect draw Tier 0/1
 *   6. DrawDynamicInstances(cmd) - direct draw Tier 2
 *   7. EndFrame()
 */
class RenderSystem {
public:
    RenderSystem() = default;
    ~RenderSystem();

    // Non-copyable
    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;

    /**
     * Initialize the render system.
     */
    bool Create(VkDevice device,
                VkPhysicalDevice physicalDevice,
                const RenderSystemConfig& config);

    /**
     * Destroy all resources.
     */
    void Destroy();

    // ---- Instance Registration ----

    /**
     * Register a static or semi-static instance (Tier 0/1).
     * Call during level load, before FinalizeStaticInstances().
     * @return Instance ID, or UINT32_MAX on failure
     */
    uint32_t RegisterStaticInstance(InstanceTier tier,
                                    const glm::mat4& transform,
                                    uint32_t meshIndex,
                                    uint32_t materialIndex,
                                    const glm::vec4& boundingSphere);

    /**
     * Update a semi-static instance transform.
     */
    void UpdateStaticTransform(uint32_t instanceId, const glm::mat4& newTransform);

    /**
     * Finalize static instances and upload to GPU.
     * Call once after all RegisterStaticInstance() calls.
     */
    bool FinalizeStaticInstances();

    // ---- Frame Flow ----

    /**
     * Begin a new frame.
     */
    void BeginFrame(uint32_t frameIndex);

    /**
     * Update view-projection for culling.
     */
    void UpdateCamera(const glm::mat4& viewProj);

    /**
     * Add a dynamic instance for this frame (Tier 2).
     * @return Index within this frame's dynamic buffer
     */
    uint32_t AddDynamicInstance(const glm::mat4& transform,
                                uint32_t meshIndex,
                                uint32_t materialIndex);

    /**
     * Dispatch GPU culling for Tier 0/1 (compute pass).
     */
    void DispatchGPUCulling(VkCommandBuffer cmd);

    /**
     * Insert barrier after GPU culling (before indirect draw).
     */
    void InsertPostCullBarrier(VkCommandBuffer cmd);

    /**
     * Draw static instances using indirect commands.
     * Requires valid graphics pipeline bound.
     */
    void DrawStaticInstances(VkCommandBuffer cmd);

    /**
     * Draw dynamic instances (direct draw calls).
     * @return Number of draw calls issued
     */
    uint32_t DrawDynamicInstances(VkCommandBuffer cmd);

    /**
     * End frame.
     */
    void EndFrame();

    /**
     * Clear all instances (for level unload).
     */
    void Clear();

    // ---- Accessors ----
    uint32_t GetStaticInstanceCount() const { return m_staticBatchManager.GetInstanceCount(); }
    uint32_t GetDynamicInstanceCount() const { return static_cast<uint32_t>(m_dynamicInstances.size()); }
    bool HasStaticDirty() const { return m_staticBatchManager.HasDirty(); }

    // For debug/stats
    StaticBatchManager& GetStaticManager() { return m_staticBatchManager; }
    GPUCuller& GetGPUCuller() { return m_gpuCuller; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    RenderSystemConfig m_config{};

    // Tier 0/1: Static and semi-static
    StaticBatchManager m_staticBatchManager;
    GPUCuller m_gpuCuller;

    // Tier 2: Dynamic instances (ring-buffered)
    struct DynamicInstance {
        GPUInstanceData instanceData;
        uint32_t meshIndex;
        uint32_t materialIndex;
    };
    std::vector<DynamicInstance> m_dynamicInstances;

    // Ring buffer for dynamic instance transforms
    RingBuffer<GPUInstanceData> m_dynamicInstanceBuffer;

    uint32_t m_currentFrame = 0;
    bool m_initialized = false;
    bool m_staticFinalized = false;
};

} // namespace render
