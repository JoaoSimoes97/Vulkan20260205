#pragma once

#include <string>
#include <vector>
#include <functional>
#include <vulkan/vulkan.h>

/**
 * Single draw: pipeline, layout, vertex buffer, optional push constants, and vkCmdDraw parameters.
 */
struct DrawCall {
    VkPipeline        pipeline         = VK_NULL_HANDLE;
    VkPipelineLayout  pipelineLayout   = VK_NULL_HANDLE;
    VkBuffer          vertexBuffer     = VK_NULL_HANDLE;
    VkDeviceSize      vertexBufferOffset = 0;
    const void*       pPushConstants   = nullptr;
    uint32_t          pushConstantSize = 0;
    uint32_t          vertexCount      = 0;
    uint32_t          instanceCount    = 1;
    uint32_t          firstVertex       = 0;
    uint32_t          firstInstance     = 0;
    /** Descriptor sets to bind (set 0, 1, ...). Empty = no descriptor sets for this pipeline. */
    std::vector<VkDescriptorSet> descriptorSets;
    /** Optional instance buffer (vertex input binding 1). When valid, instanceCount > 1 uses per-instance data. */
    VkBuffer          instanceBuffer       = VK_NULL_HANDLE;
    VkDeviceSize      instanceBufferOffset = 0;
    /** Dynamic offsets for descriptor sets (one per dynamic binding). Empty = no dynamic offsets. */
    std::vector<uint32_t> dynamicOffsets;
    
    /** Per-object data for per-viewport MVP recalculation. */
    const float*      pLocalTransform  = nullptr;  /**< Pointer to object's 4x4 model matrix (column-major). */
    float             color[4]         = {1.f, 1.f, 1.f, 1.f}; /**< Object color for push constants. */
    uint32_t          objectIndex      = 0;        /**< Object index for push constants SSBO indexing. */
    
    /** Pipeline key for per-viewport render mode switching. */
    std::string       pipelineKey;
};

/*
 * Command pool and primary command buffers (one per swapchain image).
 * Recreated when swapchain is recreated. Record() fills a buffer with render pass + list of draws.
 */
class VulkanCommandBuffers {
public:
    VulkanCommandBuffers() = default;
    ~VulkanCommandBuffers();

    void Create(VkDevice pDevice_ic, uint32_t lQueueFamilyIndex_ic, uint32_t lBufferCount_ic);
    void Destroy();

    /** Record buffer: begin render pass (renderArea, clearValues), set viewport/scissor, then for each DrawCall: bind pipeline, push constants (if any), draw.
     *  @param preSceneCallback Optional callback invoked after command buffer begin but before main render pass (for offscreen rendering).
     *  @param postSceneCallback Optional callback invoked inside render pass after main draws, for debug rendering. */
    void Record(uint32_t lIndex_ic, VkRenderPass pRenderPass_ic, VkFramebuffer pFramebuffer_ic,
                VkRect2D stRenderArea_ic, VkViewport stViewport_ic, VkRect2D stScissor_ic,
                const std::vector<DrawCall>& vecDrawCalls_ic,
                const VkClearValue* pClearValues_ic, uint32_t lClearValueCount_ic,
                std::function<void(VkCommandBuffer)> preSceneCallback = nullptr,
                std::function<void(VkCommandBuffer)> postSceneCallback = nullptr);

    VkCommandBuffer Get(uint32_t lIndex_ic) const;
    uint32_t GetCount() const { return static_cast<uint32_t>(this->m_commandBuffers.size()); }
    bool IsValid() const { return this->m_commandPool != VK_NULL_HANDLE; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
};
