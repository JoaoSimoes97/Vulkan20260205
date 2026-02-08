#include "vulkan_render_pass.h"
#include "vulkan_utils.h"
#include <stdexcept>

void VulkanRenderPass::Create(VkDevice device, VkFormat swapchainImageFormat) {
    VulkanUtils::LogTrace("VulkanRenderPass::Create");
    if (device == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("VulkanRenderPass::Create: invalid device");
        throw std::runtime_error("VulkanRenderPass::Create: invalid device");
    }
    m_device = device;

    VkAttachmentDescription colorAttachment = {
        .flags = 0,
        .format = swapchainImageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference colorRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription subpass = {
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorRef,
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr,
    };
    VkRenderPassCreateInfo rpInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 0,
        .pDependencies = nullptr,
    };
    VkResult result = vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_renderPass);
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateRenderPass failed: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to create render pass");
    }
}

void VulkanRenderPass::Destroy() {
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
    m_device = VK_NULL_HANDLE;
}

VulkanRenderPass::~VulkanRenderPass() {
    Destroy();
}
