#pragma once

#include <vulkan/vulkan.h>

/*
 * Graphics pipeline (and later: compute, raytracing). Depends on render pass and swapchain extent.
 * Future: pipeline cache, multiple pipelines, specialization constants.
 */
class VulkanPipeline {
public:
    VulkanPipeline() = default;
    ~VulkanPipeline();

    void Create(VkDevice device, VkExtent2D extent, VkRenderPass renderPass);
    void Destroy();

    VkPipeline Get() const { return m_pipeline; }
    VkPipelineLayout GetLayout() const { return m_pipelineLayout; }
    bool IsValid() const { return m_pipeline != VK_NULL_HANDLE; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
};
