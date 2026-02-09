#include "vulkan_framebuffers.h"
#include "vulkan_utils.h"
#include <stdexcept>

void VulkanFramebuffers::Create(VkDevice device, VkRenderPass renderPass,
                                const std::vector<VkImageView>& imageViews, VkExtent2D extent) {
    VulkanUtils::LogTrace("VulkanFramebuffers::Create");
    if ((device == VK_NULL_HANDLE) || (renderPass == VK_NULL_HANDLE) || (imageViews.empty() == true)) {
        VulkanUtils::LogErr("VulkanFramebuffers::Create: invalid device/renderPass/imageViews");
        throw std::runtime_error("VulkanFramebuffers::Create: invalid parameters");
    }
    m_device = device;
    m_framebuffers.resize(imageViews.size());

    for (size_t zIdx = static_cast<size_t>(0); zIdx < imageViews.size(); ++zIdx) {
        VkFramebufferCreateInfo fbInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,   /* Framebuffer create struct. */
            .pNext           = nullptr,                                     /* No extension chain. */
            .flags           = 0,                                           /* No flags. */
            .renderPass      = renderPass,                                  /* Compatible render pass. */
            .attachmentCount = 1,                                           /* Single color attachment. */
            .pAttachments    = &imageViews[zIdx],                           /* Swapchain image view for this index. */
            .width           = extent.width,                                /* Match swapchain extent. */
            .height          = extent.height,                               /* Match swapchain extent. */
            .layers          = 1,                                           /* Single layer. */
        };
        VkResult result = vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_framebuffers[zIdx]);
        if (result != VK_SUCCESS) {
            VulkanUtils::LogErr("vkCreateFramebuffer failed: {}", static_cast<int>(result));
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void VulkanFramebuffers::Destroy() {
    for (VkFramebuffer fb : m_framebuffers)
        vkDestroyFramebuffer(m_device, fb, nullptr);
    m_framebuffers.clear();
    m_device = VK_NULL_HANDLE;
}

VulkanFramebuffers::~VulkanFramebuffers() {
    Destroy();
}
