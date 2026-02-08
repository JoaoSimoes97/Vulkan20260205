#include "vulkan_config.h"
#include <cstring>

VkPresentModeKHR PresentModeFromString(const std::string& s) {
    if (s == "fifo")          return VK_PRESENT_MODE_FIFO_KHR;
    if (s == "mailbox")       return VK_PRESENT_MODE_MAILBOX_KHR;
    if (s == "immediate")     return VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (s == "fifo_relaxed")  return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    return VK_PRESENT_MODE_FIFO_KHR;
}

std::string PresentModeToString(VkPresentModeKHR eMode) {
    switch (eMode) {
        case VK_PRESENT_MODE_FIFO_KHR:         return "fifo";
        case VK_PRESENT_MODE_MAILBOX_KHR:      return "mailbox";
        case VK_PRESENT_MODE_IMMEDIATE_KHR:    return "immediate";
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "fifo_relaxed";
        default: return "fifo";
    }
}

VkFormat FormatFromString(const std::string& s) {
    if (s == "B8G8R8A8_SRGB")   return VK_FORMAT_B8G8R8A8_SRGB;
    if (s == "B8G8R8A8_UNORM")  return VK_FORMAT_B8G8R8A8_UNORM;
    if (s == "R8G8B8A8_SRGB")   return VK_FORMAT_R8G8B8A8_SRGB;
    if (s == "R8G8B8A8_UNORM")  return VK_FORMAT_R8G8B8A8_UNORM;
    return VK_FORMAT_UNDEFINED;
}

std::string FormatToString(VkFormat eFormat) {
    switch (eFormat) {
        case VK_FORMAT_B8G8R8A8_SRGB:   return "B8G8R8A8_SRGB";
        case VK_FORMAT_B8G8R8A8_UNORM:  return "B8G8R8A8_UNORM";
        case VK_FORMAT_R8G8B8A8_SRGB:   return "R8G8B8A8_SRGB";
        case VK_FORMAT_R8G8B8A8_UNORM:  return "R8G8B8A8_UNORM";
        default: return "";
    }
}

VkColorSpaceKHR ColorSpaceFromString(const std::string& s) {
    if (s == "SRGB_NONLINEAR")  return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    if (s == "DISPLAY_P3")      return VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
    if (s == "EXTENDED_SRGB")   return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
    return static_cast<VkColorSpaceKHR>(VK_COLOR_SPACE_MAX_ENUM_KHR);
}

std::string ColorSpaceToString(VkColorSpaceKHR eColorSpace) {
    switch (eColorSpace) {
        case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:           return "SRGB_NONLINEAR";
        case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:     return "DISPLAY_P3";
        case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:     return "EXTENDED_SRGB";
        default: return "";
    }
}
