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
    void Create(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                const QueueFamilyIndices& queueFamilyIndices, const VulkanConfig& config);
    void Destroy();

    /* Tear down and recreate with current extent/config (e.g. after resize or present mode change). */
    void RecreateSwapchain(const VulkanConfig& config);

    VkSwapchainKHR GetSwapchain() const { return m_swapchain; }
    const std::vector<VkImageView>& GetImageViews() const { return m_imageViews; }
    VkFormat GetImageFormat() const { return m_imageFormat; }
    VkExtent2D GetExtent() const { return m_extent; }
    uint32_t GetImageCount() const { return static_cast<uint32_t>(m_imageViews.size()); }
    bool IsValid() const { return m_swapchain != VK_NULL_HANDLE; }

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
