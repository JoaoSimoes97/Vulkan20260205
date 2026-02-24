/**
 * RenderContext — GPU resource container.
 *
 * Centralizes Vulkan device, queues, command pools, and other GPU state.
 * Passed to render passes and systems that need GPU access.
 *
 * Phase 4.3: Renderer Extraction
 */

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

/**
 * RenderContext — Holds references to core Vulkan resources.
 *
 * This is a non-owning container; resources are owned by VulkanApp or Engine.
 * Passed by const-ref to render systems that need GPU access.
 */
struct RenderContext {
    /* === Core Vulkan Handles === */
    VkInstance          instance        = VK_NULL_HANDLE;
    VkPhysicalDevice    physicalDevice  = VK_NULL_HANDLE;
    VkDevice            device          = VK_NULL_HANDLE;

    /* === Queues === */
    VkQueue             graphicsQueue   = VK_NULL_HANDLE;
    uint32_t            graphicsQueueFamily = 0;
    VkQueue             presentQueue    = VK_NULL_HANDLE;   // May be same as graphicsQueue
    uint32_t            presentQueueFamily = 0;
    VkQueue             computeQueue    = VK_NULL_HANDLE;   // Optional (future)
    uint32_t            computeQueueFamily = UINT32_MAX;    // UINT32_MAX = not available
    VkQueue             transferQueue   = VK_NULL_HANDLE;   // Optional (future)
    uint32_t            transferQueueFamily = UINT32_MAX;

    /* === Command Pools === */
    VkCommandPool       graphicsCommandPool = VK_NULL_HANDLE;
    VkCommandPool       computeCommandPool  = VK_NULL_HANDLE;   // Optional
    VkCommandPool       transferCommandPool = VK_NULL_HANDLE;   // Optional

    /* === Swapchain Info === */
    VkSurfaceKHR        surface         = VK_NULL_HANDLE;
    VkSwapchainKHR      swapchain       = VK_NULL_HANDLE;
    VkFormat            swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D          swapchainExtent = {0, 0};
    uint32_t            swapchainImageCount = 0;

    /* === Depth Buffer Info === */
    VkFormat            depthFormat     = VK_FORMAT_D32_SFLOAT;

    /* === Render Pass (main) === */
    VkRenderPass        mainRenderPass  = VK_NULL_HANDLE;

    /* === Frame Synchronization === */
    uint32_t            framesInFlight  = 2;    // Typically 2 or 3
    uint32_t            currentFrame    = 0;    // 0 to framesInFlight-1

    /* === Device Limits === */
    VkPhysicalDeviceProperties      deviceProperties{};
    VkPhysicalDeviceFeatures        deviceFeatures{};
    VkPhysicalDeviceMemoryProperties memoryProperties{};

    /**
     * Check if context is valid (all required handles are set).
     */
    bool IsValid() const {
        return device != VK_NULL_HANDLE &&
               physicalDevice != VK_NULL_HANDLE &&
               graphicsQueue != VK_NULL_HANDLE &&
               graphicsCommandPool != VK_NULL_HANDLE;
    }

    /**
     * Find memory type matching requirements.
     */
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) &&
                (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        return UINT32_MAX;
    }

    /**
     * Get maximum uniform buffer range.
     */
    VkDeviceSize GetMaxUniformBufferRange() const {
        return deviceProperties.limits.maxUniformBufferRange;
    }

    /**
     * Get maximum storage buffer range.
     */
    VkDeviceSize GetMaxStorageBufferRange() const {
        return deviceProperties.limits.maxStorageBufferRange;
    }

    /**
     * Get minimum uniform buffer offset alignment.
     */
    VkDeviceSize GetMinUniformBufferOffsetAlignment() const {
        return deviceProperties.limits.minUniformBufferOffsetAlignment;
    }

    /**
     * Get minimum storage buffer offset alignment.
     */
    VkDeviceSize GetMinStorageBufferOffsetAlignment() const {
        return deviceProperties.limits.minStorageBufferOffsetAlignment;
    }
};
