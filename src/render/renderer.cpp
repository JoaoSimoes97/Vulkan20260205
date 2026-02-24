/**
 * Renderer — Implementation.
 *
 * Phase 4.3: Renderer Extraction
 */

#include "renderer.h"
#include <algorithm>
#include <array>
#include <stdexcept>

Renderer::~Renderer() {
    if (m_initialized) {
        Destroy();
    }
}

bool Renderer::Create(const RenderContext& context) {
    if (!context.IsValid()) {
        return false;
    }

    m_context = context;
    m_framesInFlight = context.framesInFlight;

    // Create command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = context.graphicsQueueFamily;

    if (vkCreateCommandPool(context.device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        return false;
    }

    // Create frame resources (fences, semaphores, command buffers)
    if (!CreateFrameResources()) {
        Destroy();
        return false;
    }

    // Use existing render pass from context if available
    if (context.mainRenderPass != VK_NULL_HANDLE) {
        m_renderPass = context.mainRenderPass;
    } else {
        if (!CreateRenderPass()) {
            Destroy();
            return false;
        }
    }

    // Create depth resources
    if (!CreateDepthResources()) {
        Destroy();
        return false;
    }

    // Create framebuffers
    if (!CreateFramebuffers()) {
        Destroy();
        return false;
    }

    m_initialized = true;
    return true;
}

void Renderer::Destroy() {
    if (m_context.device == VK_NULL_HANDLE) {
        return;
    }

    vkDeviceWaitIdle(m_context.device);

    DestroyFramebuffers();
    DestroyDepthResources();
    
    // Only destroy render pass if we created it
    if (m_renderPass != VK_NULL_HANDLE && m_renderPass != m_context.mainRenderPass) {
        DestroyRenderPass();
    }

    DestroyFrameResources();

    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_context.device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    m_initialized = false;
}

void Renderer::OnResize(uint32_t width, uint32_t height) {
    (void)width;
    (void)height;
    m_needsRecreation = true;
}

bool Renderer::BeginFrame() {
    if (!m_initialized) {
        return false;
    }

    // If swapchain needs recreation, don't proceed
    if (m_needsRecreation) {
        return false;
    }

    FrameData& frame = m_frames[m_currentFrame];

    // Wait for this frame's fence
    vkWaitForFences(m_context.device, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);

    // Acquire swapchain image
    VkResult result = vkAcquireNextImageKHR(
        m_context.device,
        m_context.swapchain,
        UINT64_MAX,
        frame.imageAvailableSem,
        VK_NULL_HANDLE,
        &m_imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        m_needsRecreation = true;
        return false;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return false;
    }

    // Reset fence for this frame
    vkResetFences(m_context.device, 1, &frame.inFlightFence);

    // Reset and begin command buffer
    vkResetCommandBuffer(frame.commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(frame.commandBuffer, &beginInfo) != VK_SUCCESS) {
        return false;
    }

    m_currentCommandBuffer = frame.commandBuffer;
    m_stats = {}; // Reset stats for this frame

    return true;
}

bool Renderer::EndFrame() {
    if (!m_initialized || m_currentCommandBuffer == VK_NULL_HANDLE) {
        return false;
    }

    FrameData& frame = m_frames[m_currentFrame];

    // End render pass if still active
    if (m_inRenderPass) {
        EndRenderPass();
    }

    // End command buffer
    if (vkEndCommandBuffer(frame.commandBuffer) != VK_SUCCESS) {
        return false;
    }

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {frame.imageAvailableSem};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;

    VkSemaphore signalSemaphores[] = {frame.renderFinishedSem};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_context.graphicsQueue, 1, &submitInfo, frame.inFlightFence) != VK_SUCCESS) {
        return false;
    }

    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {m_context.swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &m_imageIndex;

    VkResult result = vkQueuePresentKHR(m_context.presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        m_needsRecreation = true;
    } else if (result != VK_SUCCESS) {
        return false;
    }

    // Advance to next frame
    m_currentFrame = (m_currentFrame + 1) % m_framesInFlight;
    m_currentCommandBuffer = VK_NULL_HANDLE;

    return true;
}

void Renderer::BeginMainRenderPass(float clearR, float clearG, float clearB, float clearA) {
    if (!m_initialized || m_currentCommandBuffer == VK_NULL_HANDLE || m_inRenderPass) {
        return;
    }

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{clearR, clearG, clearB, clearA}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_framebuffers[m_imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_context.swapchainExtent;
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(m_currentCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_context.swapchainExtent.width);
    viewport.height = static_cast<float>(m_context.swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(m_currentCommandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_context.swapchainExtent;
    vkCmdSetScissor(m_currentCommandBuffer, 0, 1, &scissor);

    m_inRenderPass = true;
}

void Renderer::EndRenderPass() {
    if (!m_inRenderPass || m_currentCommandBuffer == VK_NULL_HANDLE) {
        return;
    }

    vkCmdEndRenderPass(m_currentCommandBuffer);
    m_inRenderPass = false;
}

bool Renderer::RecreateSwapchain(uint32_t newWidth, uint32_t newHeight) {
    (void)newWidth;
    (void)newHeight;

    vkDeviceWaitIdle(m_context.device);

    DestroyFramebuffers();
    DestroyDepthResources();

    // Swapchain itself is recreated externally by VulkanApp
    // We just recreate our dependent resources

    if (!CreateDepthResources()) {
        return false;
    }

    if (!CreateFramebuffers()) {
        return false;
    }

    m_needsRecreation = false;
    return true;
}

bool Renderer::CreateFrameResources() {
    m_frames.resize(m_framesInFlight);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    for (uint32_t i = 0; i < m_framesInFlight; ++i) {
        FrameData& frame = m_frames[i];

        if (vkCreateFence(m_context.device, &fenceInfo, nullptr, &frame.inFlightFence) != VK_SUCCESS) {
            return false;
        }

        if (vkCreateSemaphore(m_context.device, &semaphoreInfo, nullptr, &frame.imageAvailableSem) != VK_SUCCESS) {
            return false;
        }

        if (vkCreateSemaphore(m_context.device, &semaphoreInfo, nullptr, &frame.renderFinishedSem) != VK_SUCCESS) {
            return false;
        }

        if (vkAllocateCommandBuffers(m_context.device, &allocInfo, &frame.commandBuffer) != VK_SUCCESS) {
            return false;
        }
    }

    return true;
}

void Renderer::DestroyFrameResources() {
    for (auto& frame : m_frames) {
        if (frame.inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(m_context.device, frame.inFlightFence, nullptr);
        }
        if (frame.imageAvailableSem != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_context.device, frame.imageAvailableSem, nullptr);
        }
        if (frame.renderFinishedSem != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_context.device, frame.renderFinishedSem, nullptr);
        }
        // Command buffers freed with pool
    }
    m_frames.clear();
}

bool Renderer::CreateRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_context.swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_context.depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    return vkCreateRenderPass(m_context.device, &renderPassInfo, nullptr, &m_renderPass) == VK_SUCCESS;
}

void Renderer::DestroyRenderPass() {
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_context.device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
}

bool Renderer::CreateDepthResources() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_context.swapchainExtent.width;
    imageInfo.extent.height = m_context.swapchainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_context.depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_context.device, &imageInfo, nullptr, &m_depthImage) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_context.device, m_depthImage, &memRequirements);

    uint32_t memTypeIndex = m_context.FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memTypeIndex == UINT32_MAX) {
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memTypeIndex;

    if (vkAllocateMemory(m_context.device, &allocInfo, nullptr, &m_depthMemory) != VK_SUCCESS) {
        return false;
    }

    vkBindImageMemory(m_context.device, m_depthImage, m_depthMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_context.depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    return vkCreateImageView(m_context.device, &viewInfo, nullptr, &m_depthImageView) == VK_SUCCESS;
}

void Renderer::DestroyDepthResources() {
    if (m_depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_context.device, m_depthImageView, nullptr);
        m_depthImageView = VK_NULL_HANDLE;
    }
    if (m_depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_context.device, m_depthImage, nullptr);
        m_depthImage = VK_NULL_HANDLE;
    }
    if (m_depthMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_context.device, m_depthMemory, nullptr);
        m_depthMemory = VK_NULL_HANDLE;
    }
}

bool Renderer::CreateFramebuffers() {
    // Need swapchain image views from context
    // For now, this is a placeholder - actual implementation needs
    // swapchain image views passed in context or retrieved separately
    
    m_framebuffers.resize(m_context.swapchainImageCount);

    // Note: This requires swapchain image views which need to be added to context
    // For now, return true assuming framebuffers are created externally
    return true;
}

void Renderer::DestroyFramebuffers() {
    for (auto fb : m_framebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_context.device, fb, nullptr);
        }
    }
    m_framebuffers.clear();
}
