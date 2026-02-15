/*
 * VulkanCommandBuffers — one command pool and one primary command buffer per swapchain image.
 * Record() encodes: begin render pass, set viewport/scissor, then for each DrawCall bind pipeline,
 * push constants, and vkCmdDraw; end render pass.
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

void VulkanCommandBuffers::Record(uint32_t lIndex_ic, VkRenderPass pRenderPass_ic, VkFramebuffer pFramebuffer_ic,
                                  VkRect2D stRenderArea_ic, VkViewport stViewport_ic, VkRect2D stScissor_ic,
                                  const std::vector<DrawCall>& vecDrawCalls_ic,
                                  const VkClearValue* pClearValues_ic, uint32_t lClearValueCount_ic) {
    if ((lIndex_ic >= m_commandBuffers.size()) || (pRenderPass_ic == VK_NULL_HANDLE) || (pFramebuffer_ic == VK_NULL_HANDLE)) {
        VulkanUtils::LogErr("VulkanCommandBuffers::Record: invalid index or handles");
        throw std::runtime_error("VulkanCommandBuffers::Record: invalid parameters");
    }
    if ((lClearValueCount_ic > 0) && (pClearValues_ic == nullptr)) {
        VulkanUtils::LogErr("VulkanCommandBuffers::Record: clearValueCount > 0 but pClearValues is null");
        throw std::runtime_error("VulkanCommandBuffers::Record: invalid clear values");
    }
    for (const auto& stD : vecDrawCalls_ic) {
        if ((stD.pipeline == VK_NULL_HANDLE) || (stD.pipelineLayout == VK_NULL_HANDLE) || (stD.vertexCount == 0) || (stD.vertexBuffer == VK_NULL_HANDLE)) {
            VulkanUtils::LogErr("VulkanCommandBuffers::Record: invalid DrawCall (pipeline/layout/vertexCount/vertexBuffer)");
            throw std::runtime_error("VulkanCommandBuffers::Record: invalid DrawCall");
        }
    }

    VkCommandBuffer pCmd = m_commandBuffers[lIndex_ic];

    VkResult result = vkResetCommandBuffer(pCmd, static_cast<VkCommandBufferResetFlags>(0));
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkResetCommandBuffer failed: {}", static_cast<int>(result));
        throw std::runtime_error("VulkanCommandBuffers::Record: reset failed");
    }

    VkCommandBufferBeginInfo stBeginInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext            = nullptr,
        .flags            = static_cast<VkCommandBufferUsageFlags>(0),
        .pInheritanceInfo = nullptr,
    };
    result = vkBeginCommandBuffer(pCmd, &stBeginInfo);
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkBeginCommandBuffer failed: {}", static_cast<int>(result));
        throw std::runtime_error("VulkanCommandBuffers::Record: begin failed");
    }

    VkRenderPassBeginInfo stRpBegin = {
        .sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext             = nullptr,
        .renderPass        = pRenderPass_ic,
        .framebuffer       = pFramebuffer_ic,
        .renderArea        = stRenderArea_ic,
        .clearValueCount   = lClearValueCount_ic,
        .pClearValues      = pClearValues_ic,
    };
    vkCmdBeginRenderPass(pCmd, &stRpBegin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(pCmd, 0, 1, &stViewport_ic);
    vkCmdSetScissor(pCmd, 0, 1, &stScissor_ic);

    for (const auto& stD : vecDrawCalls_ic) {
        vkCmdBindPipeline(pCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stD.pipeline);
        if (stD.vertexBuffer != VK_NULL_HANDLE)
            vkCmdBindVertexBuffers(pCmd, 0, 1, &stD.vertexBuffer, &stD.vertexBufferOffset);
        if ((stD.pPushConstants != nullptr) && (stD.pushConstantSize > 0))
            vkCmdPushConstants(pCmd, stD.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, stD.pushConstantSize, stD.pPushConstants);
        vkCmdDraw(pCmd, stD.vertexCount, stD.instanceCount, stD.firstVertex, stD.firstInstance);
    }

    vkCmdEndRenderPass(pCmd);

    result = vkEndCommandBuffer(pCmd);
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
