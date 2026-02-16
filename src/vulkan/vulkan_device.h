#pragma once

#include "vulkan_types.h"
#include <vulkan/vulkan.h>

/*
 * Physical and logical device, queue families, queues.
 * Created after instance (and optionally after surface, for present queue family).
 * Future: multiple queues (compute, transfer), device groups.
 */
class VulkanDevice {
public:
    VulkanDevice() = default;
    ~VulkanDevice();

    /* Create and pick physical device, create logical device. Optionally pass surface to set presentFamily. */
    void Create(VkInstance pInstance_ic, VkSurfaceKHR surface_ic = VK_NULL_HANDLE);
    void Destroy();

    VkPhysicalDevice GetPhysicalDevice() const { return this->m_physicalDevice; }
    VkDevice GetDevice() const { return this->m_logicalDevice; }
    VkQueue GetGraphicsQueue() const { return this->m_graphicsQueue; }
    /** Queue to use for vkQueuePresentKHR; same as graphics when presentFamily == graphicsFamily. */
    VkQueue GetPresentQueue() const { return this->m_presentQueue; }
    const QueueFamilyIndices& GetQueueFamilyIndices() const { return this->m_queueFamilyIndices; }
    bool IsValid() const { return this->m_logicalDevice != VK_NULL_HANDLE; }
    
    // Device limits (queried during Create)
    uint32_t GetMaxDescriptorSets() const { return m_limits.maxDescriptorSetSamplers; }
    uint32_t GetMaxBoundDescriptorSets() const { return m_limits.maxBoundDescriptorSets; }
    uint64_t GetMaxMemoryAllocationCount() const { return m_limits.maxMemoryAllocationCount; }
    VkDeviceSize GetMaxStorageBufferRange() const { return m_limits.maxStorageBufferRange; }
    const VkPhysicalDeviceLimits& GetLimits() const { return m_limits; }

private:
    uint32_t RateSuitability(VkPhysicalDevice pPhysicalDevice_ic, const VkPhysicalDeviceProperties& stProps_ic);
    QueueFamilyIndices FindQueueFamilyIndices(VkPhysicalDevice pPhysicalDevice_ic, VkSurfaceKHR surface_ic);

    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_logicalDevice = VK_NULL_HANDLE;
    QueueFamilyIndices m_queueFamilyIndices = {};
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue  = VK_NULL_HANDLE;
    VkPhysicalDeviceLimits m_limits = {};
};
