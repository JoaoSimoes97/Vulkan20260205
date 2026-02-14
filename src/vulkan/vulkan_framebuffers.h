#pragma once

#include <vector>
#include <vulkan/vulkan.h>

/*
 * Framebuffers (one per swapchain image view, bound to render pass).
 * Attachments: color views (one per framebuffer) + optional shared depth view.
 * Recreated when swapchain is recreated. Future: multiple render targets, shadow map framebuffers.
 */
class VulkanFramebuffers {
public:
    VulkanFramebuffers() = default;
    ~VulkanFramebuffers();

    /** colorImageViews: one per framebuffer. depthImageView: optional (VK_NULL_HANDLE = color-only). */
    void Create(VkDevice device, VkRenderPass renderPass,
                const std::vector<VkImageView>& colorImageViews,
                VkImageView depthImageView,
                VkExtent2D extent);
    void Destroy();

    const std::vector<VkFramebuffer>& Get() const { return m_framebuffers; }
    uint32_t GetCount() const { return static_cast<uint32_t>(m_framebuffers.size()); }
    bool IsValid() const { return m_framebuffers.empty() == false; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;
};
