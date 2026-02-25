#pragma once
/**
 * @file gpu_culler.h
 * @brief GPU-based frustum culling via compute shader
 * 
 * Dispatches gpu_cull.comp to cull static/semi-static instances.
 * Outputs visible instance indices and indirect draw commands.
 * 
 * See docs/instancing-architecture.md for design details.
 */

#include "instance_types.h"
#include "gpu_buffer.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace render {

/**
 * Cull uniforms uploaded to GPU.
 * Must match gpu_cull.comp CullUniforms layout.
 */
struct CullUniforms {
    glm::vec4 frustumPlanes[6];  // 6 * 16 = 96 bytes
    glm::mat4 viewProj;          // 64 bytes
    // Total: 160 bytes
};
static_assert(sizeof(CullUniforms) == 160, "CullUniforms must be 160 bytes");

/**
 * Push constants for cull dispatch.
 */
struct CullPushConstants {
    uint32_t instanceCount;
    uint32_t _pad0;
    uint32_t _pad1;
    uint32_t _pad2;
};
static_assert(sizeof(CullPushConstants) == 16, "CullPushConstants must be 16 bytes");

/**
 * GPUCuller — Compute-based frustum culling for static instances.
 * 
 * Usage:
 *   1. Create() with device and max instances
 *   2. UpdateFrustum() with camera view-projection matrix
 *   3. Dispatch() during command buffer recording
 *   4. Use output buffers for indirect drawing
 */
class GPUCuller {
public:
    GPUCuller() = default;
    ~GPUCuller();

    // Non-copyable
    GPUCuller(const GPUCuller&) = delete;
    GPUCuller& operator=(const GPUCuller&) = delete;

    /**
     * Initialize compute pipeline and buffers.
     * @param device Vulkan device
     * @param physicalDevice Physical device
     * @param maxInstances Maximum instances to cull
     * @param maxMeshes Maximum unique meshes (for indirect draw commands)
     * @param cullShaderPath Path to gpu_cull.spv
     * @return true on success
     */
    bool Create(VkDevice device,
                VkPhysicalDevice physicalDevice,
                uint32_t maxInstances,
                uint32_t maxMeshes,
                const char* cullShaderPath);

    /**
     * Destroy all resources.
     */
    void Destroy();

    /**
     * Update frustum planes from view-projection matrix.
     * Call once per frame before Dispatch().
     */
    void UpdateFrustum(const glm::mat4& viewProj);

    /**
     * Reset output counters (call at start of frame).
     * Must be called before Dispatch() each frame.
     */
    void ResetCounters(VkCommandBuffer cmd);

    /**
     * Dispatch culling compute shader.
     * @param cmd Command buffer to record into
     * @param instanceBuffer SSBO containing GPUInstanceData[]
     * @param cullDataBuffer SSBO containing GPUCullData[]
     * @param instanceCount Number of instances to cull
     */
    void Dispatch(VkCommandBuffer cmd,
                  VkBuffer instanceBuffer,
                  VkBuffer cullDataBuffer,
                  uint32_t instanceCount);

    /**
     * Insert memory barrier after dispatch (before indirect draw).
     */
    void InsertBarrier(VkCommandBuffer cmd);

    // Output buffers (for indirect drawing)
    VkBuffer GetVisibleIndicesBuffer() const { return m_visibleIndicesBuffer.GetBuffer(); }
    VkBuffer GetIndirectCommandsBuffer() const { return m_indirectCommandsBuffer.GetBuffer(); }
    
    // Get count buffer for vkCmdDrawIndexedIndirectCount
    VkBuffer GetCountBuffer() const { return m_visibleIndicesBuffer.GetBuffer(); }
    VkDeviceSize GetCountBufferOffset() const { return 0; } // count is first uint

    uint32_t GetMaxMeshes() const { return m_maxMeshes; }

private:
    bool CreateDescriptorSetLayout();
    bool CreatePipelineLayout();
    bool CreatePipeline(const char* shaderPath);
    bool CreateDescriptorPool();
    bool CreateDescriptorSet();
    bool CreateOutputBuffers();

    void ExtractFrustumPlanes(const glm::mat4& viewProj);

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    uint32_t m_maxInstances = 0;
    uint32_t m_maxMeshes = 0;

    // Compute pipeline
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    // Uniform buffer for frustum planes
    GPUBuffer m_cullUniformBuffer;
    CullUniforms m_cullUniforms{};

    // Output buffers
    GPUBuffer m_visibleIndicesBuffer;   // count + indices[]
    GPUBuffer m_indirectCommandsBuffer; // DrawIndexedIndirectCommand[]
};

} // namespace render
