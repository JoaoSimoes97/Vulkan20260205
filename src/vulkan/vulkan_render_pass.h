#pragma once

#include <vulkan/vulkan.h>

/*
 * Render pass: attachments and subpasses. Depends on swapchain format/extent.
 * Future: multiple subpasses, depth attachments, MSAA resolve, etc.
 */
class VulkanRenderPass {
public:
    VulkanRenderPass() = default;
    ~VulkanRenderPass();

    void Create(VkDevice device, VkFormat swapchainImageFormat);
    void Destroy();

    VkRenderPass Get() const { return m_renderPass; }
    bool IsValid() const { return m_renderPass != VK_NULL_HANDLE; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
};
