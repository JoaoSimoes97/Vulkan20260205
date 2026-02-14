#pragma once

#include <vector>
#include <vulkan/vulkan.h>

/**
 * Single draw: pipeline, layout, optional push constants, and vkCmdDraw parameters.
 * App builds a list of these each frame to draw multiple objects with different materials/pipelines.
 */
struct DrawCall {
    VkPipeline        pipeline         = VK_NULL_HANDLE;
    VkPipelineLayout  pipelineLayout   = VK_NULL_HANDLE;
    const void*       pPushConstants   = nullptr;  /* optional; use with pushConstantSize */
    uint32_t          pushConstantSize = 0;       /* 0 = no push constants */
    uint32_t          vertexCount      = 0;
    uint32_t          instanceCount    = 1;
    uint32_t          firstVertex      = 0;
    uint32_t          firstInstance    = 0;
};

/*
 * Command pool and primary command buffers (one per swapchain image).
 * Recreated when swapchain is recreated. Record() fills a buffer with render pass + list of draws.
 */
class VulkanCommandBuffers {
public:
    VulkanCommandBuffers() = default;
    ~VulkanCommandBuffers();

    void Create(VkDevice device, uint32_t queueFamilyIndex, uint32_t bufferCount);
    void Destroy();

    /** Record buffer: begin render pass, set viewport/scissor, then for each DrawCall: bind pipeline, push constants (if any), draw. */
    void Record(uint32_t index, VkRenderPass renderPass, VkFramebuffer framebuffer,
                VkExtent2D extent, const std::vector<DrawCall>& drawCalls,
                const VkClearValue& clearColor);

    VkCommandBuffer Get(uint32_t index) const;
    uint32_t GetCount() const { return static_cast<uint32_t>(m_commandBuffers.size()); }
    bool IsValid() const { return m_commandPool != VK_NULL_HANDLE; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
};
