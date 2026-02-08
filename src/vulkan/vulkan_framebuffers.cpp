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

    for (size_t i = 0; i < imageViews.size(); ++i) {
        VkFramebufferCreateInfo fbInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = renderPass,
            .attachmentCount = 1,
            .pAttachments = &imageViews[i],
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        };
        VkResult result = vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_framebuffers[i]);
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
