#include "vulkan_swapchain.h"
#include "vulkan_config.h"
#include "vulkan_utils.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace {

VkSurfaceFormatKHR ChooseSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                       const std::string& sPreferredFormatStr,
                                       const std::string& sPreferredColorSpaceStr) {
    uint32_t lFormatCount = static_cast<uint32_t>(0);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &lFormatCount, nullptr);
    if (lFormatCount == 0) {
        VulkanUtils::LogErr("No surface formats supported");
        throw std::runtime_error("No surface formats supported");
    }
    std::vector<VkSurfaceFormatKHR> vecFormats(lFormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &lFormatCount, vecFormats.data());

    VkFormat ePreferredFormat = (sPreferredFormatStr.empty() == true) ? VK_FORMAT_UNDEFINED : FormatFromString(sPreferredFormatStr);
    VkColorSpaceKHR ePreferredColorSpace = (sPreferredColorSpaceStr.empty() == true)
        ? static_cast<VkColorSpaceKHR>(VK_COLOR_SPACE_MAX_ENUM_KHR)
        : ColorSpaceFromString(sPreferredColorSpaceStr);

    if ((ePreferredFormat != VK_FORMAT_UNDEFINED) || (ePreferredColorSpace != static_cast<VkColorSpaceKHR>(VK_COLOR_SPACE_MAX_ENUM_KHR))) {
        for (const auto& stFmt : vecFormats) {
            bool bFormatMatch = ((ePreferredFormat == VK_FORMAT_UNDEFINED) || (stFmt.format == ePreferredFormat));
            bool bSpaceMatch = ((ePreferredColorSpace == static_cast<VkColorSpaceKHR>(VK_COLOR_SPACE_MAX_ENUM_KHR)) || (stFmt.colorSpace == ePreferredColorSpace));
            if ((bFormatMatch == true) && (bSpaceMatch == true))
                return stFmt;
        }
        std::ostringstream stmSupported;
        for (size_t zIdx = static_cast<size_t>(0); zIdx < vecFormats.size(); ++zIdx) {
            if (zIdx > static_cast<size_t>(0)) stmSupported << ", ";
            stmSupported << FormatToString(vecFormats[zIdx].format) << "+" << ColorSpaceToString(vecFormats[zIdx].colorSpace);
        }
        VulkanUtils::LogErr("Requested format '{}' color space '{}' is not supported. Supported: {}. Adjust config and restart.",
            sPreferredFormatStr.empty() == true ? "(driver default)" : sPreferredFormatStr,
            sPreferredColorSpaceStr.empty() == true ? "(driver default)" : sPreferredColorSpaceStr,
            stmSupported.str());
        throw std::runtime_error("Requested surface format/color space not supported");
    }
    /* Driver default: prefer B8G8R8A8_SRGB + SRGB_NONLINEAR, else first available */
    for (const auto& stFmt : vecFormats) {
        if ((stFmt.format == VK_FORMAT_B8G8R8A8_SRGB) && (stFmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
            return stFmt;
    }
    return vecFormats[0];
}

VkPresentModeKHR ChoosePresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                   VkPresentModeKHR ePreferred) {
    uint32_t lModeCount = static_cast<uint32_t>(0);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &lModeCount, nullptr);
    if (lModeCount == 0) {
        VulkanUtils::LogErr("No present modes supported");
        throw std::runtime_error("No present modes supported");
    }
    std::vector<VkPresentModeKHR> vecModes(lModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &lModeCount, vecModes.data());

    for (VkPresentModeKHR eMode : vecModes) {
        if (eMode == ePreferred)
            return eMode;
    }
    std::ostringstream stmSupported;
    for (size_t zIdx = static_cast<size_t>(0); zIdx < vecModes.size(); ++zIdx) {
        if (zIdx > static_cast<size_t>(0)) stmSupported << ", ";
        stmSupported << PresentModeToString(vecModes[zIdx]);
    }
    VulkanUtils::LogErr("Requested present mode '{}' is not supported. Supported: {}. Adjust config and restart.",
        PresentModeToString(ePreferred), stmSupported.str());
    throw std::runtime_error("Requested present mode not supported");
}

VkExtent2D ChooseExtent(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                        uint32_t lRequestedWidth, uint32_t lRequestedHeight) {
    VkSurfaceCapabilitiesKHR stCaps = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &stCaps);

    if (stCaps.currentExtent.width != UINT32_MAX) {
        return stCaps.currentExtent;
    }
    VkExtent2D stExtent = { lRequestedWidth, lRequestedHeight };
    stExtent.width = std::clamp(stExtent.width, stCaps.minImageExtent.width, stCaps.maxImageExtent.width);
    stExtent.height = std::clamp(stExtent.height, stCaps.minImageExtent.height, stCaps.maxImageExtent.height);
    return stExtent;
}

} // namespace

void VulkanSwapchain::Create(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                             const QueueFamilyIndices& queueFamilyIndices, const VulkanConfig& stConfig) {
    VulkanUtils::LogTrace("VulkanSwapchain::Create");
    if ((device == VK_NULL_HANDLE) || (physicalDevice == VK_NULL_HANDLE) || (surface == VK_NULL_HANDLE)) {
        VulkanUtils::LogErr("VulkanSwapchain::Create: invalid device/surface");
        throw std::runtime_error("VulkanSwapchain::Create: invalid device/surface");
    }
    this->m_device = device;
    this->m_physicalDevice = physicalDevice;
    this->m_surface = surface;
    this->m_queueFamilyIndices = queueFamilyIndices;
    this->m_config = stConfig;

    VkSurfaceFormatKHR stSurfaceFormat = ChooseSurfaceFormat(physicalDevice, surface,
                                                          stConfig.sPreferredFormat, stConfig.sPreferredColorSpace);
    VkPresentModeKHR ePresentMode = ChoosePresentMode(physicalDevice, surface, stConfig.ePresentMode);
    this->m_extent = ChooseExtent(physicalDevice, surface, stConfig.lWidth, stConfig.lHeight);
    this->m_imageFormat = stSurfaceFormat.format;

    VkSurfaceCapabilitiesKHR stCaps = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &stCaps);
    uint32_t lImageCount = stCaps.minImageCount + static_cast<uint32_t>(1);
    if ((stCaps.maxImageCount > 0) && (lImageCount > stCaps.maxImageCount))
        lImageCount = stCaps.maxImageCount;

    uint32_t lPresentFamily = (queueFamilyIndices.presentFamily != QUEUE_FAMILY_IGNORED)
                             ? queueFamilyIndices.presentFamily
                             : queueFamilyIndices.graphicsFamily;
    uint32_t lQueueFamilyIndicesArray[] = { queueFamilyIndices.graphicsFamily, lPresentFamily };
    bool bSameQueue = (queueFamilyIndices.graphicsFamily == lPresentFamily);
    uint32_t lQueueFamilyIndexCount = (bSameQueue == true) ? static_cast<uint32_t>(1) : static_cast<uint32_t>(2);

    VkSwapchainCreateInfoKHR stCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = static_cast<void*>(nullptr),
        .flags = 0,
        .surface = surface,
        .minImageCount = lImageCount,
        .imageFormat = stSurfaceFormat.format,
        .imageColorSpace = stSurfaceFormat.colorSpace,
        .imageExtent = this->m_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = (bSameQueue == true) ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = lQueueFamilyIndexCount,
        .pQueueFamilyIndices = lQueueFamilyIndicesArray,
        .preTransform = stCaps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = ePresentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    VkResult result = vkCreateSwapchainKHR(this->m_device, &stCreateInfo, nullptr, &this->m_swapchain);
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateSwapchainKHR failed: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to create swapchain");
    }

    uint32_t lSwapchainImageCount = static_cast<uint32_t>(0);
    vkGetSwapchainImagesKHR(this->m_device, this->m_swapchain, &lSwapchainImageCount, nullptr);
    this->m_images.resize(lSwapchainImageCount);
    vkGetSwapchainImagesKHR(this->m_device, this->m_swapchain, &lSwapchainImageCount, this->m_images.data());
    this->CreateImageViews();
}

void VulkanSwapchain::CreateImageViews() {
    this->m_imageViews.resize(this->m_images.size());
    for (size_t zIdx = static_cast<size_t>(0); zIdx < this->m_images.size(); ++zIdx) {
        VkImageViewCreateInfo stViewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = static_cast<void*>(nullptr),
            .flags = 0,
            .image = this->m_images[zIdx],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = this->m_imageFormat,
            .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        VkResult result = vkCreateImageView(this->m_device, &stViewInfo, nullptr, &this->m_imageViews[zIdx]);
        if (result != VK_SUCCESS) {
            VulkanUtils::LogErr("vkCreateImageView failed: {}", static_cast<int>(result));
            throw std::runtime_error("Failed to create swapchain image view");
        }
    }
}

void VulkanSwapchain::RecreateSwapchain(const VulkanConfig& stConfig) {
    VulkanUtils::LogTrace("VulkanSwapchain::RecreateSwapchain");
    this->m_config = stConfig;
    for (VkImageView imageView : this->m_imageViews)
        vkDestroyImageView(this->m_device, imageView, nullptr);
    this->m_imageViews.clear();
    this->m_images.clear();
    if (this->m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(this->m_device, this->m_swapchain, nullptr);
        this->m_swapchain = VK_NULL_HANDLE;
    }
    this->Create(this->m_device, this->m_physicalDevice, this->m_surface, this->m_queueFamilyIndices, this->m_config);
}

void VulkanSwapchain::Destroy() {
    for (VkImageView imageView : this->m_imageViews)
        vkDestroyImageView(this->m_device, imageView, nullptr);
    this->m_imageViews.clear();
    this->m_images.clear();
    if (this->m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(this->m_device, this->m_swapchain, nullptr);
        this->m_swapchain = VK_NULL_HANDLE;
    }
    this->m_device = VK_NULL_HANDLE;
    this->m_physicalDevice = VK_NULL_HANDLE;
    this->m_surface = VK_NULL_HANDLE;
    this->m_extent = { static_cast<uint32_t>(0), static_cast<uint32_t>(0) };
    this->m_imageFormat = VK_FORMAT_UNDEFINED;
}

VulkanSwapchain::~VulkanSwapchain() {
    this->Destroy();
}
