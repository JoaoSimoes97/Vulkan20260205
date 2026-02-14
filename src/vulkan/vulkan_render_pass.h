#pragma once

#include <vulkan/vulkan.h>

/**
 * Descriptor for render pass creation. Drives attachment list and subpass; no hardcoded formats or ops.
 * Use depthFormat = VK_FORMAT_UNDEFINED for color-only pass.
 */
struct RenderPassDescriptor {
    VkFormat              colorFormat;
    VkAttachmentLoadOp    colorLoadOp   = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp   colorStoreOp  = VK_ATTACHMENT_STORE_OP_STORE;
    VkImageLayout         colorFinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkFormat              depthFormat   = VK_FORMAT_UNDEFINED;  /* no depth if UNDEFINED */
    VkAttachmentLoadOp    depthLoadOp   = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp   depthStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkImageLayout         depthFinalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkSampleCountFlagBits sampleCount   = VK_SAMPLE_COUNT_1_BIT;
};

/*
 * Render pass: attachments and subpasses driven by RenderPassDescriptor.
 * Future: multiple subpasses, MSAA resolve, etc.
 */
class VulkanRenderPass {
public:
    VulkanRenderPass() = default;
    ~VulkanRenderPass();

    void Create(VkDevice device, const RenderPassDescriptor& descriptor);
    void Destroy();

    VkRenderPass Get() const { return m_renderPass; }
    bool IsValid() const { return m_renderPass != VK_NULL_HANDLE; }
    /** True if the render pass was created with a depth attachment (pipeline must provide depth state). */
    bool HasDepthAttachment() const { return m_hasDepth; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    bool m_hasDepth = false;
};
