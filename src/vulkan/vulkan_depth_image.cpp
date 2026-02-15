#include "vulkan_depth_image.h"
#include "vulkan_utils.h"
#include <stdexcept>

namespace {

uint32_t FindMemoryType(VkPhysicalDevice pPhysicalDevice_ic, uint32_t lTypeFilter_ic, VkMemoryPropertyFlags uProperties_ic) {
    VkPhysicalDeviceMemoryProperties stMemProps = {};
    vkGetPhysicalDeviceMemoryProperties(pPhysicalDevice_ic, &stMemProps);
    for (uint32_t lIdx = static_cast<uint32_t>(0); lIdx < stMemProps.memoryTypeCount; ++lIdx) {
        if (((lTypeFilter_ic & (1u << lIdx)) != 0u) &&
            ((stMemProps.memoryTypes[lIdx].propertyFlags & uProperties_ic) == uProperties_ic))
            return lIdx;
    }
    throw std::runtime_error("VulkanDepthImage: no suitable memory type");
}

bool HasStencilComponent(VkFormat eFormat_ic) {
    return (eFormat_ic == VK_FORMAT_D32_SFLOAT_S8_UINT) || (eFormat_ic == VK_FORMAT_D24_UNORM_S8_UINT);
}

} // namespace

VkFormat VulkanDepthImage::FindSupportedFormat(VkPhysicalDevice pPhysicalDevice_ic,
                                               const VkFormat* pCandidates_ic, uint32_t lCandidateCount_ic) {
    for (uint32_t lIdx = static_cast<uint32_t>(0); lIdx < lCandidateCount_ic; ++lIdx) {
        VkFormat eFormat = pCandidates_ic[lIdx];
        VkFormatProperties stProps = {};
        vkGetPhysicalDeviceFormatProperties(pPhysicalDevice_ic, eFormat, &stProps);

        VkImageFormatProperties stImgProps = {};
        VkResult r = vkGetPhysicalDeviceImageFormatProperties(pPhysicalDevice_ic, eFormat,
            VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, static_cast<VkImageCreateFlags>(0), &stImgProps);
        if (r != VK_SUCCESS)
            continue;

        const VkFormatFeatureFlags uRequired = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if ((stProps.optimalTilingFeatures & uRequired) == uRequired)
            return eFormat;
    }
    return VK_FORMAT_UNDEFINED;
}

void VulkanDepthImage::Create(VkDevice pDevice_ic, VkPhysicalDevice pPhysicalDevice_ic,
                              VkFormat eDepthFormat_ic, VkExtent2D stExtent_ic) {
    VulkanUtils::LogTrace("VulkanDepthImage::Create");
    if ((pDevice_ic == VK_NULL_HANDLE) || (pPhysicalDevice_ic == VK_NULL_HANDLE) ||
        (eDepthFormat_ic == VK_FORMAT_UNDEFINED) || (stExtent_ic.width == 0) || (stExtent_ic.height == 0)) {
        VulkanUtils::LogErr("VulkanDepthImage::Create: invalid parameters");
        throw std::runtime_error("VulkanDepthImage::Create: invalid parameters");
    }
    Destroy();
    this->m_device = pDevice_ic;
    this->m_format = eDepthFormat_ic;

    VkImageCreateInfo stImageInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = eDepthFormat_ic,
        .extent        = { stExtent_ic.width, stExtent_ic.height, 1 },
        .mipLevels     = static_cast<uint32_t>(1),
        .arrayLayers   = static_cast<uint32_t>(1),
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = static_cast<uint32_t>(0),
        .pQueueFamilyIndices   = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkResult r = vkCreateImage(pDevice_ic, &stImageInfo, nullptr, &this->m_image);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateImage (depth) failed: {}", static_cast<int>(r));
        throw std::runtime_error("VulkanDepthImage::Create: image failed");
    }

    VkMemoryRequirements stMemReqs = {};
    vkGetImageMemoryRequirements(pDevice_ic, this->m_image, &stMemReqs);
    VkMemoryAllocateInfo stAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = stMemReqs.size,
        .memoryTypeIndex = FindMemoryType(pPhysicalDevice_ic, stMemReqs.memoryTypeBits,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    r = vkAllocateMemory(pDevice_ic, &stAllocInfo, nullptr, &this->m_memory);
    if (r != VK_SUCCESS) {
        vkDestroyImage(pDevice_ic, this->m_image, nullptr);
        this->m_image = VK_NULL_HANDLE;
        VulkanUtils::LogErr("vkAllocateMemory (depth) failed: {}", static_cast<int>(r));
        throw std::runtime_error("VulkanDepthImage::Create: memory failed");
    }
    vkBindImageMemory(pDevice_ic, this->m_image, this->m_memory, static_cast<VkDeviceSize>(0));

    VkImageViewCreateInfo stViewInfo = {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .image    = this->m_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = eDepthFormat_ic,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask     = static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | (HasStencilComponent(eDepthFormat_ic) == true ? VK_IMAGE_ASPECT_STENCIL_BIT : static_cast<VkImageAspectFlagBits>(0))),
            .baseMipLevel   = static_cast<uint32_t>(0),
            .levelCount     = static_cast<uint32_t>(1),
            .baseArrayLayer = static_cast<uint32_t>(0),
            .layerCount     = static_cast<uint32_t>(1),
        },
    };
    r = vkCreateImageView(pDevice_ic, &stViewInfo, nullptr, &this->m_view);
    if (r != VK_SUCCESS) {
        vkFreeMemory(pDevice_ic, this->m_memory, nullptr);
        vkDestroyImage(pDevice_ic, this->m_image, nullptr);
        this->m_memory = VK_NULL_HANDLE;
        this->m_image  = VK_NULL_HANDLE;
        VulkanUtils::LogErr("vkCreateImageView (depth) failed: {}", static_cast<int>(r));
        throw std::runtime_error("VulkanDepthImage::Create: view failed");
    }
}

void VulkanDepthImage::Destroy() {
    if (this->m_device == VK_NULL_HANDLE)
        return;
    if (this->m_view != VK_NULL_HANDLE) {
        vkDestroyImageView(this->m_device, this->m_view, nullptr);
        this->m_view = VK_NULL_HANDLE;
    }
    if (this->m_image != VK_NULL_HANDLE) {
        vkDestroyImage(this->m_device, this->m_image, nullptr);
        this->m_image = VK_NULL_HANDLE;
    }
    if (this->m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(this->m_device, this->m_memory, nullptr);
        this->m_memory = VK_NULL_HANDLE;
    }
    this->m_device = VK_NULL_HANDLE;
    this->m_format = VK_FORMAT_UNDEFINED;
}

VulkanDepthImage::~VulkanDepthImage() {
    Destroy();
}
