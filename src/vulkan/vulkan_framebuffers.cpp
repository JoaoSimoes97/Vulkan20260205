#include "vulkan_framebuffers.h"
#include "vulkan_utils.h"
#include <stdexcept>
#include <vector>

void VulkanFramebuffers::Create(VkDevice pDevice_ic, VkRenderPass renderPass_ic,
                                const std::vector<VkImageView>& vecColorImageViews_ic,
                                VkImageView depthImageView_ic,
                                VkExtent2D stExtent_ic) {
    VulkanUtils::LogTrace("VulkanFramebuffers::Create");
    if ((pDevice_ic == VK_NULL_HANDLE) || (renderPass_ic == VK_NULL_HANDLE) || (vecColorImageViews_ic.empty() == true)) {
        VulkanUtils::LogErr("VulkanFramebuffers::Create: invalid device/renderPass/colorImageViews");
        throw std::runtime_error("VulkanFramebuffers::Create: invalid parameters");
    }
    this->m_device = pDevice_ic;
    this->m_framebuffers.resize(vecColorImageViews_ic.size());
    const bool bUseDepth = (depthImageView_ic != VK_NULL_HANDLE);

    for (size_t zIdx = static_cast<size_t>(0); zIdx < vecColorImageViews_ic.size(); ++zIdx) {
        std::vector<VkImageView> vecAttachments;
        vecAttachments.push_back(vecColorImageViews_ic[zIdx]);
        if (bUseDepth == true)
            vecAttachments.push_back(depthImageView_ic);

        VkFramebufferCreateInfo stFbInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .renderPass      = renderPass_ic,
            .attachmentCount = static_cast<uint32_t>(vecAttachments.size()),
            .pAttachments    = vecAttachments.data(),
            .width           = stExtent_ic.width,
            .height          = stExtent_ic.height,
            .layers          = static_cast<uint32_t>(1),
        };
        VkResult r = vkCreateFramebuffer(this->m_device, &stFbInfo, nullptr, &this->m_framebuffers[zIdx]);
        if (r != VK_SUCCESS) {
            VulkanUtils::LogErr("vkCreateFramebuffer failed: {}", static_cast<int>(r));
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void VulkanFramebuffers::Destroy() {
    for (VkFramebuffer pFb : this->m_framebuffers)
        vkDestroyFramebuffer(this->m_device, pFb, nullptr);
    this->m_framebuffers.clear();
    this->m_device = VK_NULL_HANDLE;
}

VulkanFramebuffers::~VulkanFramebuffers() {
    Destroy();
}
