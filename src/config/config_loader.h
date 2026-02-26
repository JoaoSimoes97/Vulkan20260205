#pragma once

#include "vulkan_config.h"
#include <string>
#include <vulkan/vulkan.h>

/*
 * Two-file config: default (immutable) + user (mutable).
 * - default.json: created once if missing, never overwritten. Single source of default values.
 * - config.json: user config; created from default if missing; can be updated by the app.
 * Load merges user over default (missing keys in user = value from default).
 * JSON keys: window, swapchain, camera (use_perspective, fov_y_rad, near_z, far_z, ortho_*, pan_speed), render (clear_color_r/g/b/a).
 * Validation layers are not persisted. See docs/architecture.md.
 */
VulkanConfig GetDefaultConfig();

/** Validate config values and clamp out-of-range values. Returns true if all values were valid. */
bool ValidateConfig(VulkanConfig& stConfig);

/** Validate config against GPU device limits. Call after device creation. Returns true if all values valid. */
bool ValidateConfigGPULimits(VulkanConfig& stConfig, const VkPhysicalDeviceLimits& limits);

/** Ensure default config file exists; create from GetDefaultConfig() only if missing. Never overwrites. */
void EnsureDefaultConfigFile(const std::string& sDefaultPath_ic);

/** Load user config merged over default. If user file missing, create it from default and return default. */
VulkanConfig LoadConfigFromFileOrCreate(const std::string& sUserPath_ic, const std::string& sDefaultPath_ic);

VulkanConfig LoadConfigFromFile(const std::string& sPath_ic);
void SaveConfigToFile(const std::string& sPath_ic, const VulkanConfig& stConfig_ic);
