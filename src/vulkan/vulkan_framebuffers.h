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
    void Create(VkDevice pDevice_ic, VkRenderPass renderPass_ic,
                const std::vector<VkImageView>& vecColorImageViews_ic,
                VkImageView depthImageView_ic,
                VkExtent2D stExtent_ic);
    void Destroy();

    const std::vector<VkFramebuffer>& Get() const { return this->m_framebuffers; }
    uint32_t GetCount() const { return static_cast<uint32_t>(this->m_framebuffers.size()); }
    bool IsValid() const { return (this->m_framebuffers.empty() == false); }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;
};
