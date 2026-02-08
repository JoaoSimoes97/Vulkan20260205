#pragma once

#include <vector>
#include <vulkan/vulkan.h>

/*
 * Framebuffers (one per swapchain image view, bound to render pass).
 * Recreated when swapchain is recreated. Future: multiple render targets, shadow map framebuffers.
 */
class VulkanFramebuffers {
public:
    VulkanFramebuffers() = default;
    ~VulkanFramebuffers();

    void Create(VkDevice device, VkRenderPass renderPass,
                const std::vector<VkImageView>& imageViews, VkExtent2D extent);
    void Destroy();

    const std::vector<VkFramebuffer>& Get() const { return m_framebuffers; }
    uint32_t GetCount() const { return static_cast<uint32_t>(m_framebuffers.size()); }
    bool IsValid() const { return m_framebuffers.empty() == false; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;
};
