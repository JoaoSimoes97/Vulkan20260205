#include "vulkan_swapchain.h"
#include "vulkan_config.h"
#include "vulkan_utils.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

/* Helpers: surface format, present mode, extent selection. */
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
    if ((lRequestedWidth == 0u) || (lRequestedHeight == 0u)) {
        VulkanUtils::LogErr("ChooseExtent: requested extent {}x{} is invalid; caller must supply non-zero size.",
            lRequestedWidth, lRequestedHeight);
        throw std::runtime_error("ChooseExtent: zero extent not allowed");
    }

    VkSurfaceCapabilitiesKHR stCaps = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &stCaps);

    const uint32_t minW = stCaps.minImageExtent.width;
    const uint32_t minH = stCaps.minImageExtent.height;
    const uint32_t maxW = stCaps.maxImageExtent.width;
    const uint32_t maxH = stCaps.maxImageExtent.height;

    /* If requested size is within surface limits, use it exactly (no aspect change). */
    if ((lRequestedWidth >= minW) && (lRequestedWidth <= maxW) && (lRequestedHeight >= minH) && (lRequestedHeight <= maxH))
        return VkExtent2D{ lRequestedWidth, lRequestedHeight };

    /* Otherwise fit into [min,max] preserving aspect ratio so the image is never stretched. */
    double scaleMaxW = (maxW > 0u) ? static_cast<double>(maxW) / static_cast<double>(lRequestedWidth) : 1.0;
    double scaleMaxH = (maxH > 0u) ? static_cast<double>(maxH) / static_cast<double>(lRequestedHeight) : 1.0;
    double scaleMinW = (lRequestedWidth > 0u) ? static_cast<double>(minW) / static_cast<double>(lRequestedWidth) : 0.0;
    double scaleMinH = (lRequestedHeight > 0u) ? static_cast<double>(minH) / static_cast<double>(lRequestedHeight) : 0.0;

    double scale = std::min(scaleMaxW, scaleMaxH);
    double scaleMin = std::max(scaleMinW, scaleMinH);
    if (scale < scaleMin)
        scale = scaleMin;

    uint32_t extentW = static_cast<uint32_t>(std::round(static_cast<double>(lRequestedWidth) * scale));
    uint32_t extentH = static_cast<uint32_t>(std::round(static_cast<double>(lRequestedHeight) * scale));
    extentW = std::clamp(extentW, minW, maxW);
    extentH = std::clamp(extentH, minH, maxH);

    VulkanUtils::LogWarn("Swapchain extent adjusted from requested {}x{} to {}x{} (surface min/max, aspect preserved).",
        lRequestedWidth, lRequestedHeight, extentW, extentH);
    return VkExtent2D{ extentW, extentH };
}

} // namespace

void VulkanSwapchain::Create(VkDevice pDevice_ic, VkPhysicalDevice pPhysicalDevice_ic, VkSurfaceKHR surface_ic,
                             const QueueFamilyIndices& stQueueFamilyIndices_ic, const VulkanConfig& stConfig_ic) {
    VulkanUtils::LogTrace("VulkanSwapchain::Create");
    if ((pDevice_ic == VK_NULL_HANDLE) || (pPhysicalDevice_ic == VK_NULL_HANDLE) || (surface_ic == VK_NULL_HANDLE)) {
        VulkanUtils::LogErr("VulkanSwapchain::Create: invalid device/surface");
        throw std::runtime_error("VulkanSwapchain::Create: invalid device/surface");
    }
    this->m_device = pDevice_ic;
    this->m_physicalDevice = pPhysicalDevice_ic;
    this->m_surface = surface_ic;
    this->m_queueFamilyIndices = stQueueFamilyIndices_ic;
    this->m_config = stConfig_ic;

    VkSurfaceFormatKHR stSurfaceFormat = ChooseSurfaceFormat(pPhysicalDevice_ic, surface_ic,
                                                          stConfig_ic.sPreferredFormat, stConfig_ic.sPreferredColorSpace);
    VkPresentModeKHR ePresentMode = ChoosePresentMode(pPhysicalDevice_ic, surface_ic, stConfig_ic.ePresentMode);
    this->m_extent = ChooseExtent(pPhysicalDevice_ic, surface_ic, stConfig_ic.lWidth, stConfig_ic.lHeight);
    this->m_imageFormat = stSurfaceFormat.format;

    /* Surface caps: validate config image count; no clamping, fail if invalid. */
    VkSurfaceCapabilitiesKHR stCaps = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pPhysicalDevice_ic, surface_ic, &stCaps);
    uint32_t lRequestedCount = stConfig_ic.lImageCount;
    if (lRequestedCount < stCaps.minImageCount) {
        VulkanUtils::LogErr("Config image_count {} is below surface minImageCount {}. Adjust config and restart.", lRequestedCount, stCaps.minImageCount);
        throw std::runtime_error("VulkanSwapchain::Create: image_count below surface minimum");
    }
    if ((stCaps.maxImageCount > 0) && (lRequestedCount > stCaps.maxImageCount)) {
        VulkanUtils::LogErr("Config image_count {} exceeds surface maxImageCount {}. Adjust config and restart.", lRequestedCount, stCaps.maxImageCount);
        throw std::runtime_error("VulkanSwapchain::Create: image_count above surface maximum");
    }

    /* Use present queue if distinct from graphics; else same queue for both. */
    uint32_t lPresentFamily = (stQueueFamilyIndices_ic.presentFamily != QUEUE_FAMILY_IGNORED)
                             ? stQueueFamilyIndices_ic.presentFamily
                             : stQueueFamilyIndices_ic.graphicsFamily;
    uint32_t lQueueFamilyIndicesArray[] = { stQueueFamilyIndices_ic.graphicsFamily, lPresentFamily };
    bool bSameQueue = (stQueueFamilyIndices_ic.graphicsFamily == lPresentFamily);
    uint32_t lQueueFamilyIndexCount = (bSameQueue == true) ? static_cast<uint32_t>(1) : static_cast<uint32_t>(2);

    VkSwapchainCreateInfoKHR stCreateInfo = {
        .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,                                   /* KHR swapchain create. */
        .pNext                 = static_cast<void*>(nullptr),                                                   /* No extension chain. */
        .flags                 = 0,                                                                             /* No create flags. */
        .surface               = surface_ic,                                                                     /* Target presentation surface. */
        .minImageCount         = lRequestedCount,                                                               /* Exact count from config; validated against caps. */
        .imageFormat           = stSurfaceFormat.format,                                                        /* Chosen surface format. */
        .imageColorSpace       = stSurfaceFormat.colorSpace,                                                    /* Chosen color space. */
        .imageExtent           = this->m_extent,                                                                /* Swapchain image dimensions. */
        .imageArrayLayers      = 1,                                                                             /* Single layer (2D). */
        .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,                                           /* Render to and present. */
        .imageSharingMode      = (bSameQueue == true) ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT, /* Exclusive if same queue family. */
        .queueFamilyIndexCount = lQueueFamilyIndexCount,                                                        /* 1 or 2 families. */
        .pQueueFamilyIndices   = lQueueFamilyIndicesArray,                                                      /* Graphics and optionally present. */
        .preTransform          = stCaps.currentTransform,                                                       /* Use surface current transform. */
        .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,                                             /* Opaque (no alpha). */
        .presentMode           = ePresentMode,                                                                  /* Config preferred or driver default. */
        .clipped               = VK_TRUE,                                                                       /* Allow clipping for better performance. */
        .oldSwapchain          = VK_NULL_HANDLE,                                                                /* New swapchain (not recreation). */
    };

    VkResult result = vkCreateSwapchainKHR(this->m_device, &stCreateInfo, nullptr, &this->m_swapchain);
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateSwapchainKHR failed: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to create swapchain");
    }

    uint32_t lSwapchainImageCount = static_cast<uint32_t>(0);
    vkGetSwapchainImagesKHR(this->m_device, this->m_swapchain, &lSwapchainImageCount, nullptr);
    if (lSwapchainImageCount != lRequestedCount) {
        vkDestroySwapchainKHR(this->m_device, this->m_swapchain, nullptr);
        this->m_swapchain = VK_NULL_HANDLE;
        VulkanUtils::LogErr("Config image_count {} not satisfied: driver returned {} images. Adjust config and restart.", lRequestedCount, lSwapchainImageCount);
        throw std::runtime_error("VulkanSwapchain::Create: driver returned different image count than config");
    }
    this->m_images.resize(lSwapchainImageCount);
    vkGetSwapchainImagesKHR(this->m_device, this->m_swapchain, &lSwapchainImageCount, this->m_images.data());
    VulkanUtils::LogInfo("Swapchain image count: {} ({} buffering).", lSwapchainImageCount, lSwapchainImageCount == 2 ? "double" : (lSwapchainImageCount == 3 ? "triple" : "other"));
    this->CreateImageViews();
}

void VulkanSwapchain::CreateImageViews() {
    this->m_imageViews.resize(this->m_images.size());
    for (size_t zIdx = static_cast<size_t>(0); zIdx < this->m_images.size(); ++zIdx) {
        VkImageViewCreateInfo stViewInfo = {
            .sType   = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,                                    /* Image view create struct. */
            .pNext   = static_cast<void*>(nullptr),                                                 /* No extension chain. */
            .flags   = 0,                                                                           /* No flags. */
            .image   = this->m_images[zIdx],                                                        /* Swapchain image for this index. */
            .viewType = VK_IMAGE_VIEW_TYPE_2D,                                                      /* 2D color target. */
            .format  = this->m_imageFormat,                                                         /* Match swapchain format. */
            .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,     /* Identity swizzle. */
                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },   /* Identity swizzle. */
            .subresourceRange = {
                .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,                                       /* Color aspect. */
                .baseMipLevel    = 0,                                                               /* First mip. */
                .levelCount      = 1,                                                               /* Single mip level. */
                .baseArrayLayer  = 0,                                                               /* First layer. */
                .layerCount      = 1,                                                               /* Single array layer. */
            },
        };
        VkResult result = vkCreateImageView(this->m_device, &stViewInfo, nullptr, &this->m_imageViews[zIdx]);
        if (result != VK_SUCCESS) {
            VulkanUtils::LogErr("vkCreateImageView failed: {}", static_cast<int>(result));
            throw std::runtime_error("Failed to create swapchain image view");
        }
    }
}

void VulkanSwapchain::RecreateSwapchain(const VulkanConfig& stConfig_ic) {
    VulkanUtils::LogTrace("VulkanSwapchain::RecreateSwapchain");
    this->m_config = stConfig_ic;
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
