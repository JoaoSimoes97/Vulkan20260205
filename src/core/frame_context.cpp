/**
 * FrameContext — Implementation of per-frame resource management.
 *
 * Phase 4.1: Ring-Buffered GPU Resources
 */

#include "frame_context.h"
#include <stdexcept>
#include <cstring>

bool FrameContextManager::Create(VkDevice device, uint32_t queueFamilyIndex, uint32_t framesInFlight) {
    if (device == VK_NULL_HANDLE || framesInFlight == 0) {
        return false;
    }

    m_device = device;
    m_framesInFlight = framesInFlight;
    m_currentFrameIndex = 0;
    m_frames.resize(framesInFlight);

    // Create command pool for this manager
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndex;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        return false;
    }

    // Create synchronization primitives and command buffers for each frame
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so first wait succeeds

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    for (uint32_t i = 0; i < framesInFlight; ++i) {
        FrameContext& frame = m_frames[i];

        // Create fence
        if (vkCreateFence(device, &fenceInfo, nullptr, &frame.inFlightFence) != VK_SUCCESS) {
            Destroy();
            return false;
        }

        // Create semaphores
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.imageAvailableSemaphore) != VK_SUCCESS) {
            Destroy();
            return false;
        }

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.renderFinishedSemaphore) != VK_SUCCESS) {
            Destroy();
            return false;
        }

        // Allocate command buffer
        if (vkAllocateCommandBuffers(device, &allocInfo, &frame.commandBuffer) != VK_SUCCESS) {
            Destroy();
            return false;
        }

        frame.valid = true;
    }

    return true;
}

void FrameContextManager::Destroy() {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }

    // Wait for all frames to complete before destroying
    WaitAll(m_device);

    for (auto& frame : m_frames) {
        if (frame.inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(m_device, frame.inFlightFence, nullptr);
            frame.inFlightFence = VK_NULL_HANDLE;
        }
        if (frame.imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, frame.imageAvailableSemaphore, nullptr);
            frame.imageAvailableSemaphore = VK_NULL_HANDLE;
        }
        if (frame.renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, frame.renderFinishedSemaphore, nullptr);
            frame.renderFinishedSemaphore = VK_NULL_HANDLE;
        }
        // Command buffers are freed when the command pool is destroyed
        frame.commandBuffer = VK_NULL_HANDLE;
        frame.valid = false;
    }

    // Destroy command pool (we own it)
    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    m_frames.clear();
    m_device = VK_NULL_HANDLE;
    m_framesInFlight = 0;
    m_currentFrameIndex = 0;
}

void FrameContextManager::WaitAll(VkDevice device) {
    if (device == VK_NULL_HANDLE) {
        return;
    }

    std::vector<VkFence> fences;
    fences.reserve(m_frames.size());

    for (const auto& frame : m_frames) {
        if (frame.inFlightFence != VK_NULL_HANDLE) {
            fences.push_back(frame.inFlightFence);
        }
    }

    if (!fences.empty()) {
        vkWaitForFences(device, static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, UINT64_MAX);
    }
}
