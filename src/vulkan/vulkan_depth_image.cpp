#include "vulkan_depth_image.h"
#include "vulkan_utils.h"
#include <stdexcept>

namespace {

uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("VulkanDepthImage: no suitable memory type");
}

bool HasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

} // namespace

VkFormat VulkanDepthImage::FindSupportedFormat(VkPhysicalDevice physicalDevice,
                                               const VkFormat* candidates, uint32_t candidateCount) {
    for (uint32_t i = 0; i < candidateCount; ++i) {
        VkFormat format = candidates[i];
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        VkImageFormatProperties imgProps;
        VkResult r = vkGetPhysicalDeviceImageFormatProperties(physicalDevice, format,
            VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &imgProps);
        if (r != VK_SUCCESS)
            continue;

        const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if ((props.optimalTilingFeatures & required) == required)
            return format;
    }
    return VK_FORMAT_UNDEFINED;
}

void VulkanDepthImage::Create(VkDevice device, VkPhysicalDevice physicalDevice,
                              VkFormat depthFormat, VkExtent2D extent) {
    VulkanUtils::LogTrace("VulkanDepthImage::Create");
    if (device == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE ||
        depthFormat == VK_FORMAT_UNDEFINED || extent.width == 0 || extent.height == 0) {
        VulkanUtils::LogErr("VulkanDepthImage::Create: invalid parameters");
        throw std::runtime_error("VulkanDepthImage::Create: invalid parameters");
    }
    Destroy();
    m_device = device;
    m_format = depthFormat;

    VkImageCreateInfo imageInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = depthFormat,
        .extent        = { extent.width, extent.height, 1 },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkResult result = vkCreateImage(device, &imageInfo, nullptr, &m_image);
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateImage (depth) failed: {}", static_cast<int>(result));
        throw std::runtime_error("VulkanDepthImage::Create: image failed");
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_image, &memReqs);
    VkMemoryAllocateInfo allocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = FindMemoryType(physicalDevice, memReqs.memoryTypeBits,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    result = vkAllocateMemory(device, &allocInfo, nullptr, &m_memory);
    if (result != VK_SUCCESS) {
        vkDestroyImage(device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        VulkanUtils::LogErr("vkAllocateMemory (depth) failed: {}", static_cast<int>(result));
        throw std::runtime_error("VulkanDepthImage::Create: memory failed");
    }
    vkBindImageMemory(device, m_image, m_memory, 0);

    VkImageViewCreateInfo viewInfo = {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .image    = m_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = depthFormat,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask     = static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | (HasStencilComponent(depthFormat) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)),
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };
    result = vkCreateImageView(device, &viewInfo, nullptr, &m_view);
    if (result != VK_SUCCESS) {
        vkFreeMemory(device, m_memory, nullptr);
        vkDestroyImage(device, m_image, nullptr);
        m_memory = VK_NULL_HANDLE;
        m_image  = VK_NULL_HANDLE;
        VulkanUtils::LogErr("vkCreateImageView (depth) failed: {}", static_cast<int>(result));
        throw std::runtime_error("VulkanDepthImage::Create: view failed");
    }
}

void VulkanDepthImage::Destroy() {
    if (m_device == VK_NULL_HANDLE)
        return;
    if (m_view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_view, nullptr);
        m_view = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
    m_device = VK_NULL_HANDLE;
    m_format = VK_FORMAT_UNDEFINED;
}

VulkanDepthImage::~VulkanDepthImage() {
    Destroy();
}
