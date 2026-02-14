#pragma once

#include <vulkan/vulkan.h>

/*
 * Depth image + view for use as a render pass attachment. Created from (device, physical device, format, extent).
 * Recreate when extent changes. Caller passes the view into framebuffer creation.
 */
class VulkanDepthImage {
public:
    VulkanDepthImage() = default;
    ~VulkanDepthImage();

    void Create(VkDevice device, VkPhysicalDevice physicalDevice, VkFormat depthFormat, VkExtent2D extent);
    void Destroy();

    VkImageView GetView() const { return m_view; }
    VkFormat GetFormat() const { return m_format; }
    bool IsValid() const { return m_view != VK_NULL_HANDLE; }

    /** Pick a supported depth format (e.g. D32_SFLOAT or D24_UNORM_S8_UINT). Returns VK_FORMAT_UNDEFINED if none. */
    static VkFormat FindSupportedFormat(VkPhysicalDevice physicalDevice,
                                       const VkFormat* candidates, uint32_t candidateCount);

private:
    VkDevice       m_device = VK_NULL_HANDLE;
    VkImage        m_image  = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkImageView    m_view   = VK_NULL_HANDLE;
    VkFormat       m_format = VK_FORMAT_UNDEFINED;
};
