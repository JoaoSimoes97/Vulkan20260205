/*
 * VulkanCommandBuffers — one command pool and one primary command buffer per swapchain image.
 * Record() encodes: begin render pass, bind pipeline, vkCmdDraw(3) for the fullscreen triangle, end.
 */
#include "vulkan_command_buffers.h"
#include "vulkan_utils.h"
#include <stdexcept>

void VulkanCommandBuffers::Create(VkDevice device, uint32_t queueFamilyIndex, uint32_t bufferCount) {
    VulkanUtils::LogTrace("VulkanCommandBuffers::Create");
    if (device == VK_NULL_HANDLE || bufferCount == 0) {
        VulkanUtils::LogErr("VulkanCommandBuffers::Create: invalid device or bufferCount");
        throw std::runtime_error("VulkanCommandBuffers::Create: invalid parameters");
    }

    m_device = device;

    VkCommandPoolCreateInfo poolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };

    VkResult result = vkCreateCommandPool(device, &poolInfo, nullptr, &m_commandPool);
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateCommandPool failed: {}", static_cast<int>(result));
        throw std::runtime_error("VulkanCommandBuffers::Create: command pool failed");
    }

    m_commandBuffers.resize(bufferCount);
    VkCommandBufferAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = m_commandPool,
        .level               = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = bufferCount,
    };

    result = vkAllocateCommandBuffers(device, &allocInfo, m_commandBuffers.data());
    if (result != VK_SUCCESS) {
        vkDestroyCommandPool(device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
        m_commandBuffers.clear();
        VulkanUtils::LogErr("vkAllocateCommandBuffers failed: {}", static_cast<int>(result));
        throw std::runtime_error("VulkanCommandBuffers::Create: allocate failed");
    }
}

void VulkanCommandBuffers::Destroy() {
    if (m_device != VK_NULL_HANDLE && m_commandPool != VK_NULL_HANDLE && m_commandBuffers.empty() == false) {
        vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
        m_commandBuffers.clear();
    }
    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }
    m_device = VK_NULL_HANDLE;
}

void VulkanCommandBuffers::Record(uint32_t index, VkRenderPass renderPass, VkFramebuffer framebuffer,
                                  VkExtent2D extent, const std::vector<DrawCall>& drawCalls,
                                  const VkClearValue& clearColor) {
    if (index >= m_commandBuffers.size() || renderPass == VK_NULL_HANDLE || framebuffer == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("VulkanCommandBuffers::Record: invalid index or handles");
        throw std::runtime_error("VulkanCommandBuffers::Record: invalid parameters");
    }
    for (const auto& d : drawCalls) {
        if (d.pipeline == VK_NULL_HANDLE || d.pipelineLayout == VK_NULL_HANDLE || d.vertexCount == 0) {
            VulkanUtils::LogErr("VulkanCommandBuffers::Record: invalid DrawCall (pipeline/layout/vertexCount)");
            throw std::runtime_error("VulkanCommandBuffers::Record: invalid DrawCall");
        }
    }

    VkCommandBuffer cmd = m_commandBuffers[index];

    VkResult result = vkResetCommandBuffer(cmd, static_cast<VkCommandBufferResetFlags>(0));
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkResetCommandBuffer failed: {}", static_cast<int>(result));
        throw std::runtime_error("VulkanCommandBuffers::Record: reset failed");
    }

    VkCommandBufferBeginInfo beginInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext            = nullptr,
        .flags            = static_cast<VkCommandBufferUsageFlags>(0),
        .pInheritanceInfo = nullptr,
    };
    result = vkBeginCommandBuffer(cmd, &beginInfo);
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkBeginCommandBuffer failed: {}", static_cast<int>(result));
        throw std::runtime_error("VulkanCommandBuffers::Record: begin failed");
    }

    VkRenderPassBeginInfo rpBegin = {
        .sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext             = nullptr,
        .renderPass        = renderPass,
        .framebuffer       = framebuffer,
        .renderArea       = { .offset = { 0, 0 }, .extent = extent },
        .clearValueCount  = 1,
        .pClearValues     = &clearColor,
    };
    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = static_cast<float>(extent.width),
        .height   = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor = { .offset = { 0, 0 }, .extent = extent };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    for (const auto& d : drawCalls) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, d.pipeline);
        if (d.pPushConstants != nullptr && d.pushConstantSize > 0)
            vkCmdPushConstants(cmd, d.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, d.pushConstantSize, d.pPushConstants);
        vkCmdDraw(cmd, d.vertexCount, d.instanceCount, d.firstVertex, d.firstInstance);
    }

    vkCmdEndRenderPass(cmd);

    result = vkEndCommandBuffer(cmd);
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkEndCommandBuffer failed: {}", static_cast<int>(result));
        throw std::runtime_error("VulkanCommandBuffers::Record: end failed");
    }
}

VkCommandBuffer VulkanCommandBuffers::Get(uint32_t index) const {
    if (index >= m_commandBuffers.size())
        return VK_NULL_HANDLE;
    return m_commandBuffers[index];
}

VulkanCommandBuffers::~VulkanCommandBuffers() {
    Destroy();
}
