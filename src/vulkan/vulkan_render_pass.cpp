#include "vulkan_render_pass.h"
#include "vulkan_utils.h"
#include <array>
#include <stdexcept>
#include <vector>

void VulkanRenderPass::Create(VkDevice device, const RenderPassDescriptor& descriptor) {
    VulkanUtils::LogTrace("VulkanRenderPass::Create");
    if (device == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("VulkanRenderPass::Create: invalid device");
        throw std::runtime_error("VulkanRenderPass::Create: invalid device");
    }
    m_device = device;
    m_hasDepth = (descriptor.depthFormat != VK_FORMAT_UNDEFINED);

    VkAttachmentDescription colorAttachment = {
        .flags          = 0,
        .format         = descriptor.colorFormat,
        .samples        = descriptor.sampleCount,
        .loadOp         = descriptor.colorLoadOp,
        .storeOp        = descriptor.colorStoreOp,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = descriptor.colorFinalLayout,
    };

    std::vector<VkAttachmentDescription> attachments;
    attachments.push_back(colorAttachment);

    VkAttachmentReference colorRef = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depthRef = {};
    VkSubpassDescription subpass = {
        .flags                   = 0,
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount    = 0,
        .pInputAttachments       = nullptr,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &colorRef,
        .pResolveAttachments     = nullptr,
        .pDepthStencilAttachment = nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments    = nullptr,
    };

    if (m_hasDepth) {
        VkAttachmentDescription depthAttachment = {
            .flags          = 0,
            .format         = descriptor.depthFormat,
            .samples        = descriptor.sampleCount,
            .loadOp         = descriptor.depthLoadOp,
            .storeOp        = descriptor.depthStoreOp,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = descriptor.depthFinalLayout,
        };
        attachments.push_back(depthAttachment);
        depthRef = {
            .attachment = 1,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        subpass.pDepthStencilAttachment = &depthRef;
    }

    // Subpass dependencies for layout transitions - must match ViewportManager's offscreen render pass
    // for pipeline compatibility (dependencyCount must be equal for compatible render passes)
    std::array<VkSubpassDependency, 2> dependencies{};
    
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo rpInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = 0,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments    = attachments.data(),
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = static_cast<uint32_t>(dependencies.size()),
        .pDependencies   = dependencies.data(),
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
