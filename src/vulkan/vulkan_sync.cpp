/*
 * VulkanSync — fences and semaphores for acquire / submit / present. Render-finished semaphores
 * are per swapchain image to satisfy Vulkan reuse rules; image-available and fences are per frame-in-flight.
 */
#include "vulkan_sync.h"
#include "vulkan_utils.h"
#include <stdexcept>

void VulkanSync::Create(VkDevice pDevice_ic, uint32_t lMaxFramesInFlight_ic, uint32_t lSwapchainImageCount_ic) {
    VulkanUtils::LogTrace("VulkanSync::Create");
    if ((pDevice_ic == VK_NULL_HANDLE) || (lMaxFramesInFlight_ic == 0) || (lSwapchainImageCount_ic == 0)) {
        VulkanUtils::LogErr("VulkanSync::Create: invalid device, maxFramesInFlight, or swapchainImageCount");
        throw std::runtime_error("VulkanSync::Create: invalid parameters");
    }
    this->m_device = pDevice_ic;
    this->m_maxFramesInFlight = lMaxFramesInFlight_ic;
    this->m_currentFrame = static_cast<uint32_t>(0);

    this->m_imageAvailableSemaphores.resize(lMaxFramesInFlight_ic);
    this->m_renderFinishedSemaphores.resize(lSwapchainImageCount_ic);
    this->m_inFlightFences.resize(lMaxFramesInFlight_ic);

    VkSemaphoreCreateInfo stSemaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    VkFenceCreateInfo stFenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (uint32_t lIdx = static_cast<uint32_t>(0); lIdx < lMaxFramesInFlight_ic; ++lIdx) {
        VkResult r = vkCreateSemaphore(pDevice_ic, &stSemaphoreInfo, nullptr, &this->m_imageAvailableSemaphores[lIdx]);
        if (r != VK_SUCCESS) {
            for (uint32_t lJ = static_cast<uint32_t>(0); lJ < lIdx; ++lJ) {
                vkDestroySemaphore(pDevice_ic, this->m_imageAvailableSemaphores[lJ], nullptr);
            }
            VulkanUtils::LogErr("vkCreateSemaphore imageAvailable failed: {}", static_cast<int>(r));
            throw std::runtime_error("VulkanSync::Create: image available semaphore failed");
        }

        r = vkCreateFence(pDevice_ic, &stFenceInfo, nullptr, &this->m_inFlightFences[lIdx]);
        if (r != VK_SUCCESS) {
            vkDestroySemaphore(pDevice_ic, this->m_imageAvailableSemaphores[lIdx], nullptr);
            for (uint32_t lJ = static_cast<uint32_t>(0); lJ < lIdx; ++lJ) {
                vkDestroySemaphore(pDevice_ic, this->m_imageAvailableSemaphores[lJ], nullptr);
                vkDestroyFence(pDevice_ic, this->m_inFlightFences[lJ], nullptr);
            }
            VulkanUtils::LogErr("vkCreateFence failed: {}", static_cast<int>(r));
            throw std::runtime_error("VulkanSync::Create: fence failed");
        }
    }

    for (uint32_t lIdx = static_cast<uint32_t>(0); lIdx < lSwapchainImageCount_ic; ++lIdx) {
        VkResult r = vkCreateSemaphore(pDevice_ic, &stSemaphoreInfo, nullptr, &this->m_renderFinishedSemaphores[lIdx]);
        if (r != VK_SUCCESS) {
            for (uint32_t lJ = static_cast<uint32_t>(0); lJ < lMaxFramesInFlight_ic; ++lJ) {
                vkDestroySemaphore(pDevice_ic, this->m_imageAvailableSemaphores[lJ], nullptr);
                vkDestroyFence(pDevice_ic, this->m_inFlightFences[lJ], nullptr);
            }
            for (uint32_t lJ = static_cast<uint32_t>(0); lJ < lIdx; ++lJ) {
                vkDestroySemaphore(pDevice_ic, this->m_renderFinishedSemaphores[lJ], nullptr);
            }
            VulkanUtils::LogErr("vkCreateSemaphore renderFinished failed: {}", static_cast<int>(r));
            throw std::runtime_error("VulkanSync::Create: render finished semaphore failed");
        }
    }
}

void VulkanSync::Destroy() {
    if (this->m_device == VK_NULL_HANDLE)
        return;
    for (size_t zIdx = static_cast<size_t>(0); zIdx < this->m_inFlightFences.size(); ++zIdx) {
        vkDestroyFence(this->m_device, this->m_inFlightFences[zIdx], nullptr);
        vkDestroySemaphore(this->m_device, this->m_imageAvailableSemaphores[zIdx], nullptr);
    }
    for (size_t zIdx = static_cast<size_t>(0); zIdx < this->m_renderFinishedSemaphores.size(); ++zIdx) {
        vkDestroySemaphore(this->m_device, this->m_renderFinishedSemaphores[zIdx], nullptr);
    }
    this->m_inFlightFences.clear();
    this->m_imageAvailableSemaphores.clear();
    this->m_renderFinishedSemaphores.clear();
    this->m_maxFramesInFlight = static_cast<uint32_t>(0);
    this->m_currentFrame = static_cast<uint32_t>(0);
    this->m_device = VK_NULL_HANDLE;
}

VkFence VulkanSync::GetInFlightFence(uint32_t lFrameIndex_ic) const {
    if (lFrameIndex_ic >= this->m_inFlightFences.size())
        return VK_NULL_HANDLE;
    return this->m_inFlightFences[lFrameIndex_ic];
}

VkSemaphore VulkanSync::GetImageAvailableSemaphore(uint32_t lFrameIndex_ic) const {
    if (lFrameIndex_ic >= this->m_imageAvailableSemaphores.size())
        return VK_NULL_HANDLE;
    return this->m_imageAvailableSemaphores[lFrameIndex_ic];
}

VkSemaphore VulkanSync::GetRenderFinishedSemaphore(uint32_t lImageIndex_ic) const {
    if (lImageIndex_ic >= this->m_renderFinishedSemaphores.size())
        return VK_NULL_HANDLE;
    return this->m_renderFinishedSemaphores[lImageIndex_ic];
}

VulkanSync::~VulkanSync() {
    Destroy();
}
