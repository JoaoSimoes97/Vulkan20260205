#include "vulkan_framebuffers.h"
#include "vulkan_utils.h"
#include <stdexcept>
#include <vector>

void VulkanFramebuffers::Create(VkDevice device, VkRenderPass renderPass,
                                const std::vector<VkImageView>& colorImageViews,
                                VkImageView depthImageView,
                                VkExtent2D extent) {
    VulkanUtils::LogTrace("VulkanFramebuffers::Create");
    if ((device == VK_NULL_HANDLE) || (renderPass == VK_NULL_HANDLE) || (colorImageViews.empty() == true)) {
        VulkanUtils::LogErr("VulkanFramebuffers::Create: invalid device/renderPass/colorImageViews");
        throw std::runtime_error("VulkanFramebuffers::Create: invalid parameters");
    }
    m_device = device;
    m_framebuffers.resize(colorImageViews.size());
    const bool useDepth = (depthImageView != VK_NULL_HANDLE);

    for (size_t zIdx = static_cast<size_t>(0); zIdx < colorImageViews.size(); ++zIdx) {
        std::vector<VkImageView> attachments;
        attachments.push_back(colorImageViews[zIdx]);
        if (useDepth)
            attachments.push_back(depthImageView);

        VkFramebufferCreateInfo fbInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .renderPass      = renderPass,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments    = attachments.data(),
            .width           = extent.width,
            .height          = extent.height,
            .layers          = 1,
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
