#pragma once

#include <cstdint>
#include <string>
#include <vulkan/vulkan.h>

/*
 * All editable Vulkan/display options. Persisted as JSON (see config_loader.h).
 * Canonical default values live in GetDefaultConfig() in config_loader.cpp; header initializers are fallbacks.
 * Apply at runtime via VulkanApp::ApplyConfig(); swapchain-related changes trigger recreate next frame.
 */
struct VulkanConfig {
    /* --- Window --- */
    uint32_t lWidth  = static_cast<uint32_t>(800);
    uint32_t lHeight = static_cast<uint32_t>(600);
    bool bFullscreen = static_cast<bool>(false);
    std::string sWindowTitle = "Vulkan App";

    /* --- Swapchain --- */
    VkPresentModeKHR ePresentMode = VK_PRESENT_MODE_FIFO_KHR;
    /* Preferred surface format (e.g. "B8G8R8A8_SRGB"). Empty = driver default. */
    std::string sPreferredFormat = "B8G8R8A8_SRGB";
    /* Preferred color space (e.g. "SRGB_NONLINEAR"). Empty = driver default. */
    std::string sPreferredColorSpace = "SRGB_NONLINEAR";

    /* Dev/debug only: not persisted in config file. Set from build type or env when implementing. */
    bool bValidationLayers = static_cast<bool>(false);

    /* Internal: do not persist. Set by ApplyConfig or resize path. */
    bool bSwapchainDirty = static_cast<bool>(false);
};

/* Present mode: string <-> VkPresentModeKHR */
VkPresentModeKHR PresentModeFromString(const std::string& s);
std::string PresentModeToString(VkPresentModeKHR eMode);

/* Surface format: string <-> VkFormat (common swapchain formats). Unknown string returns VK_FORMAT_UNDEFINED. */
VkFormat FormatFromString(const std::string& s);
std::string FormatToString(VkFormat eFormat);

/* Color space: string <-> VkColorSpaceKHR. Unknown string returns VK_COLOR_SPACE_MAX_ENUM_KHR. */
VkColorSpaceKHR ColorSpaceFromString(const std::string& s);
std::string ColorSpaceToString(VkColorSpaceKHR eColorSpace);
