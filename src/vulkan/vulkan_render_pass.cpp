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
        .flags          = 0,                                        /* No flags. */
        .format         = swapchainImageFormat,                     /* Match swapchain image format. */
        .samples        = VK_SAMPLE_COUNT_1_BIT,                    /* No MSAA. */
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,              /* Clear before render. */
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,             /* Store for present. */
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,          /* No stencil. */
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,         /* No stencil store. */
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,                /* Don't care before first use. */
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,          /* Optimal layout for presentation. */
    };

    VkAttachmentReference colorRef = {
        .attachment = 0,                                            /* Index of color attachment in render pass. */
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,     /* Optimal layout during subpass. */
    };

    VkSubpassDescription subpass = {
        .flags                   = 0,                               /* No flags. */
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS, /* Graphics subpass. */
        .inputAttachmentCount    = 0,                               /* No input attachments. */
        .pInputAttachments       = nullptr,                         /* No input attachments. */
        .colorAttachmentCount    = 1,                               /* One color output. */
        .pColorAttachments       = &colorRef,                       /* Our color attachment. */
        .pResolveAttachments     = nullptr,                         /* No resolve. */
        .pDepthStencilAttachment = nullptr,                         /* No depth/stencil. */
        .preserveAttachmentCount = 0,                               /* No preserved attachments. */
        .pPreserveAttachments    = nullptr,
    };

    VkRenderPassCreateInfo rpInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,   /* Render pass create. */
        .pNext           = nullptr,                                     /* No extension chain. */
        .flags           = 0,                                           /* No flags. */
        .attachmentCount = 1,                                           /* One color attachment. */
        .pAttachments     = &colorAttachment,                           /* Our color attachment. */
        .subpassCount     = 1,                                          /* Single subpass. */
        .pSubpasses       = &subpass,                                   /* Our single subpass. */
        .dependencyCount  = 0,                                          /* No subpass dependencies. */
        .pDependencies    = nullptr,
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
