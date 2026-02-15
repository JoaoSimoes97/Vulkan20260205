#pragma once

#include "vulkan_config.h"
#include "vulkan_types.h"
#include <vector>
#include <vulkan/vulkan.h>

/*
 * Swapchain and swapchain image views. RecreateSwapchain() tears down and recreates
 * (used on resize, present mode change, config change). Future: format, color space, HDR.
 */
class VulkanSwapchain {
public:
    VulkanSwapchain() = default;
    ~VulkanSwapchain();

    /* Create initial swapchain (call after device and surface exist). */
    void Create(VkDevice pDevice_ic, VkPhysicalDevice pPhysicalDevice_ic, VkSurfaceKHR surface_ic,
                const QueueFamilyIndices& stQueueFamilyIndices_ic, const VulkanConfig& stConfig_ic);
    void Destroy();

    /* Tear down and recreate with current extent/config (e.g. after resize or present mode change). */
    void RecreateSwapchain(const VulkanConfig& stConfig_ic);

    VkSwapchainKHR GetSwapchain() const { return this->m_swapchain; }
    const std::vector<VkImageView>& GetImageViews() const { return this->m_imageViews; }
    VkFormat GetImageFormat() const { return this->m_imageFormat; }
    VkExtent2D GetExtent() const { return this->m_extent; }
    uint32_t GetImageCount() const { return static_cast<uint32_t>(this->m_imageViews.size()); }
    bool IsValid() const { return this->m_swapchain != VK_NULL_HANDLE; }

private:
    void CreateImageViews();

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    QueueFamilyIndices m_queueFamilyIndices = {};
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    VkFormat m_imageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent = { 0, 0 };
    VulkanConfig m_config = {};
};
