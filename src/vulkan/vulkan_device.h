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
    void Create(VkInstance instance, VkSurfaceKHR surface = VK_NULL_HANDLE);
    void Destroy();

    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VkDevice GetDevice() const { return m_logicalDevice; }
    VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
    const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_queueFamilyIndices; }
    bool IsValid() const { return m_logicalDevice != VK_NULL_HANDLE; }

private:
    uint32_t RateSuitability(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceProperties& props);
    QueueFamilyIndices FindQueueFamilyIndices(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_logicalDevice = VK_NULL_HANDLE;
    QueueFamilyIndices m_queueFamilyIndices = {};
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
};
