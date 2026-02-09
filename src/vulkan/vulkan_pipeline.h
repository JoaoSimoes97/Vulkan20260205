#pragma once

#include "vulkan_shader_manager.h"
#include <vulkan/vulkan.h>
#include <string>

/*
 * Graphics pipeline (and later: compute, raytracing). Depends on render pass and swapchain extent.
 * Loads vert/frag via VulkanShaderManager; holds refs until Destroy.
 */
class VulkanPipeline {
public:
    VulkanPipeline() = default;
    ~VulkanPipeline();

    void Create(VkDevice device, VkExtent2D extent, VkRenderPass renderPass,
                VulkanShaderManager* pShaderManager,
                const std::string& sVertPath, const std::string& sFragPath);
    void Destroy();

    VkPipeline Get() const { return this->m_pipeline; }
    VkPipelineLayout GetLayout() const { return this->m_pipelineLayout; }
    bool IsValid() const { return this->m_pipeline != VK_NULL_HANDLE; }

private:
    VkDevice       m_device = VK_NULL_HANDLE;
    VkPipeline     m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VulkanShaderManager* m_pShaderManager = nullptr;
    std::string    m_sVertPath;
    std::string    m_sFragPath;
};
