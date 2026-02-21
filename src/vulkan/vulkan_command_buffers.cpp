/*
 * VulkanCommandBuffers — one command pool and one primary command buffer per swapchain image.
 * Record() encodes: begin render pass, set viewport/scissor, then for each DrawCall bind pipeline,
 * push constants, and vkCmdDraw; end render pass.
 */
#include "vulkan_command_buffers.h"
#include "vulkan_utils.h"
#include <stdexcept>

void VulkanCommandBuffers::Create(VkDevice pDevice_ic, uint32_t lQueueFamilyIndex_ic, uint32_t lBufferCount_ic) {
    VulkanUtils::LogTrace("VulkanCommandBuffers::Create");
    if ((pDevice_ic == VK_NULL_HANDLE) || (lBufferCount_ic == 0)) {
        VulkanUtils::LogErr("VulkanCommandBuffers::Create: invalid device or bufferCount");
        throw std::runtime_error("VulkanCommandBuffers::Create: invalid parameters");
    }

    this->m_device = pDevice_ic;

    VkCommandPoolCreateInfo stPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = lQueueFamilyIndex_ic,
    };

    VkResult r = vkCreateCommandPool(pDevice_ic, &stPoolInfo, nullptr, &this->m_commandPool);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateCommandPool failed: {}", static_cast<int>(r));
        throw std::runtime_error("VulkanCommandBuffers::Create: command pool failed");
    }

    this->m_commandBuffers.resize(lBufferCount_ic);
    VkCommandBufferAllocateInfo stAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = this->m_commandPool,
        .level               = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = lBufferCount_ic,
    };

    r = vkAllocateCommandBuffers(pDevice_ic, &stAllocInfo, this->m_commandBuffers.data());
    if (r != VK_SUCCESS) {
        vkDestroyCommandPool(pDevice_ic, this->m_commandPool, nullptr);
        this->m_commandPool = VK_NULL_HANDLE;
        this->m_commandBuffers.clear();
        VulkanUtils::LogErr("vkAllocateCommandBuffers failed: {}", static_cast<int>(r));
        throw std::runtime_error("VulkanCommandBuffers::Create: allocate failed");
    }
}

void VulkanCommandBuffers::Destroy() {
    if ((this->m_device != VK_NULL_HANDLE) && (this->m_commandPool != VK_NULL_HANDLE) && (this->m_commandBuffers.empty() == false)) {
        vkFreeCommandBuffers(this->m_device, this->m_commandPool, static_cast<uint32_t>(this->m_commandBuffers.size()), this->m_commandBuffers.data());
        this->m_commandBuffers.clear();
    }
    if (this->m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(this->m_device, this->m_commandPool, nullptr);
        this->m_commandPool = VK_NULL_HANDLE;
    }
    this->m_device = VK_NULL_HANDLE;
}

void VulkanCommandBuffers::Record(uint32_t lIndex_ic, VkRenderPass pRenderPass_ic, VkFramebuffer pFramebuffer_ic,
                                  VkRect2D stRenderArea_ic, VkViewport stViewport_ic, VkRect2D stScissor_ic,
                                  const std::vector<DrawCall>& vecDrawCalls_ic,
                                  const VkClearValue* pClearValues_ic, uint32_t lClearValueCount_ic,
                                  std::function<void(VkCommandBuffer)> preSceneCallback,
                                  std::function<void(VkCommandBuffer)> postSceneCallback) {
    if ((lIndex_ic >= this->m_commandBuffers.size()) || (pRenderPass_ic == VK_NULL_HANDLE) || (pFramebuffer_ic == VK_NULL_HANDLE)) {
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

    VkCommandBuffer pCmd = this->m_commandBuffers[lIndex_ic];

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

    // Pre-scene callback (for offscreen/PIP viewport rendering)
    if (preSceneCallback) {
        preSceneCallback(pCmd);
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
        if (!stD.descriptorSets.empty()) {
            /* Bind descriptor sets with or without dynamic offsets. */
            if (!stD.dynamicOffsets.empty()) {
                vkCmdBindDescriptorSets(pCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stD.pipelineLayout, 0,
                                        static_cast<uint32_t>(stD.descriptorSets.size()), stD.descriptorSets.data(),
                                        static_cast<uint32_t>(stD.dynamicOffsets.size()), stD.dynamicOffsets.data());
            } else {
                vkCmdBindDescriptorSets(pCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stD.pipelineLayout, 0,
                                        static_cast<uint32_t>(stD.descriptorSets.size()), stD.descriptorSets.data(), 0, nullptr);
            }
        }
        if (stD.vertexBuffer != VK_NULL_HANDLE || stD.instanceBuffer != VK_NULL_HANDLE) {
            VkBuffer buffers[2] = { stD.vertexBuffer, stD.instanceBuffer };
            VkDeviceSize offsets[2] = { stD.vertexBufferOffset, stD.instanceBufferOffset };
            uint32_t firstBinding = 0;
            uint32_t bindCount;
            if (stD.vertexBuffer != VK_NULL_HANDLE && stD.instanceBuffer != VK_NULL_HANDLE) {
                bindCount = 2u;
            } else if (stD.vertexBuffer != VK_NULL_HANDLE) {
                bindCount = 1u;
            } else {
                firstBinding = 1u;
                buffers[0] = stD.instanceBuffer;
                offsets[0] = stD.instanceBufferOffset;
                bindCount = 1u;
            }
            vkCmdBindVertexBuffers(pCmd, firstBinding, bindCount, buffers, offsets);
        }
        if ((stD.pPushConstants != nullptr) && (stD.pushConstantSize > 0))
            vkCmdPushConstants(pCmd, stD.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, stD.pushConstantSize, stD.pPushConstants);
        vkCmdDraw(pCmd, stD.vertexCount, stD.instanceCount, stD.firstVertex, stD.firstInstance);
    }

    /* Post-scene callback for debug rendering (inside render pass). */
    if (postSceneCallback) {
        postSceneCallback(pCmd);
    }

    vkCmdEndRenderPass(pCmd);

    result = vkEndCommandBuffer(pCmd);
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkEndCommandBuffer failed: {}", static_cast<int>(result));
        throw std::runtime_error("VulkanCommandBuffers::Record: end failed");
    }
}

VkCommandBuffer VulkanCommandBuffers::Get(uint32_t lIndex_ic) const {
    if (lIndex_ic >= this->m_commandBuffers.size())
        return VK_NULL_HANDLE;
    return this->m_commandBuffers[lIndex_ic];
}

VulkanCommandBuffers::~VulkanCommandBuffers() {
    Destroy();
}
