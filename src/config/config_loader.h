#pragma once

#include "vulkan_config.h"
#include <string>

/*
 * Two-file config: default (immutable) + user (mutable).
 * - default.json: created once if missing, never overwritten. Single source of default values.
 * - config.json: user config; created from default if missing; can be updated by the app.
 * Load merges user over default (missing keys in user = value from default).
 * JSON keys: window (width, height, fullscreen, title), swapchain (present_mode, preferred_format, preferred_color_space).
 * Validation layers are not persisted. See docs/architecture.md.
 */
VulkanConfig GetDefaultConfig();

/** Ensure default config file exists; create from GetDefaultConfig() only if missing. Never overwrites. */
void EnsureDefaultConfigFile(const std::string& sDefaultPath);

/** Load user config merged over default. If user file missing, create it from default and return default. */
VulkanConfig LoadConfigFromFileOrCreate(const std::string& sUserPath, const std::string& sDefaultPath);

VulkanConfig LoadConfigFromFile(const std::string& sPath);
void SaveConfigToFile(const std::string& sPath, const VulkanConfig& stConfig);
