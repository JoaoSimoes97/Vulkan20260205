/*
 * VulkanSync — fences and semaphores for acquire / submit / present. Render-finished semaphores
 * are per swapchain image to satisfy Vulkan reuse rules; image-available and fences are per frame-in-flight.
 */
#include "vulkan_sync.h"
#include "vulkan_utils.h"
#include <stdexcept>

void VulkanSync::Create(VkDevice device, uint32_t maxFramesInFlight, uint32_t swapchainImageCount) {
    VulkanUtils::LogTrace("VulkanSync::Create");
    if (device == VK_NULL_HANDLE || maxFramesInFlight == 0 || swapchainImageCount == 0) {
        VulkanUtils::LogErr("VulkanSync::Create: invalid device, maxFramesInFlight, or swapchainImageCount");
        throw std::runtime_error("VulkanSync::Create: invalid parameters");
    }
    m_device = device;
    m_maxFramesInFlight = maxFramesInFlight;
    m_currentFrame = 0;

    m_imageAvailableSemaphores.resize(maxFramesInFlight);
    m_renderFinishedSemaphores.resize(swapchainImageCount);
    m_inFlightFences.resize(maxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (uint32_t i = 0; i < maxFramesInFlight; ++i)
    {
        VkResult result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]);
        if (result != VK_SUCCESS)
        {
            for (uint32_t j = 0; j < i; ++j) {
                vkDestroySemaphore(device, m_imageAvailableSemaphores[j], nullptr);
            }
            VulkanUtils::LogErr("vkCreateSemaphore imageAvailable failed: {}", static_cast<int>(result));
            throw std::runtime_error("VulkanSync::Create: image available semaphore failed");
        }

        result = vkCreateFence(device, &fenceInfo, nullptr, &m_inFlightFences[i]);
        if (result != VK_SUCCESS)
        {
            vkDestroySemaphore(device, m_imageAvailableSemaphores[i], nullptr);
            for (uint32_t j = 0; j < i; ++j) {
                vkDestroySemaphore(device, m_imageAvailableSemaphores[j], nullptr);
                vkDestroyFence(device, m_inFlightFences[j], nullptr);
            }
            VulkanUtils::LogErr("vkCreateFence failed: {}", static_cast<int>(result));
            throw std::runtime_error("VulkanSync::Create: fence failed");
        }
    }

    for (uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        VkResult result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]);
        if (result != VK_SUCCESS)
        {
            for (uint32_t j = 0; j < maxFramesInFlight; ++j) {
                vkDestroySemaphore(device, m_imageAvailableSemaphores[j], nullptr);
                vkDestroyFence(device, m_inFlightFences[j], nullptr);
            }
            for (uint32_t j = 0; j < i; ++j) {
                vkDestroySemaphore(device, m_renderFinishedSemaphores[j], nullptr);
            }
            VulkanUtils::LogErr("vkCreateSemaphore renderFinished failed: {}", static_cast<int>(result));
            throw std::runtime_error("VulkanSync::Create: render finished semaphore failed");
        }
    }
}

void VulkanSync::Destroy() {
    if (m_device == VK_NULL_HANDLE)
        return;
    for (size_t i = 0; i < m_inFlightFences.size(); ++i) {
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
    }
    for (size_t i = 0; i < m_renderFinishedSemaphores.size(); ++i) {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
    }
    m_inFlightFences.clear();
    m_imageAvailableSemaphores.clear();
    m_renderFinishedSemaphores.clear();
    m_maxFramesInFlight = 0;
    m_currentFrame = 0;
    m_device = VK_NULL_HANDLE;
}

VkFence VulkanSync::GetInFlightFence(uint32_t frameIndex) const {
    if (frameIndex >= m_inFlightFences.size())
        return VK_NULL_HANDLE;
    return m_inFlightFences[frameIndex];
}

VkSemaphore VulkanSync::GetImageAvailableSemaphore(uint32_t frameIndex) const {
    if (frameIndex >= m_imageAvailableSemaphores.size())
        return VK_NULL_HANDLE;
    return m_imageAvailableSemaphores[frameIndex];
}

VkSemaphore VulkanSync::GetRenderFinishedSemaphore(uint32_t imageIndex) const {
    if (imageIndex >= m_renderFinishedSemaphores.size())
        return VK_NULL_HANDLE;
    return m_renderFinishedSemaphores[imageIndex];
}

VulkanSync::~VulkanSync() {
    Destroy();
}
