#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

/*
 * Shared Vulkan types used across modules (device, swapchain, etc.).
 * Keep this minimal; device/swapchain-specific structs stay in their modules.
 */

/* Sentinel for "queue family not found". */
constexpr uint32_t QUEUE_FAMILY_IGNORED = UINT32_MAX;

struct QueueFamilyIndices {
    uint32_t graphicsFamily = QUEUE_FAMILY_IGNORED;
    uint32_t presentFamily  = QUEUE_FAMILY_IGNORED;
};
