#pragma once

#include "vulkan_config.h"
#include <string>

/*
 * Config is always loaded from a JSON file. Defaults live in code (GetDefaultConfig()).
 * If the file does not exist, it is created from those defaults and a log message is emitted;
 * the user can edit the file before the next run.
 *
 * JSON keys: window (width, height, fullscreen, title), swapchain (present_mode, preferred_format, preferred_color_space).
 * Validation layers are not persisted. See docs/architecture.md.
 */
VulkanConfig GetDefaultConfig();

/** Load from path; if file does not exist, create it from GetDefaultConfig(), log, and return defaults. */
VulkanConfig LoadConfigFromFileOrCreate(const std::string& sPath);

VulkanConfig LoadConfigFromFile(const std::string& sPath);
void SaveConfigToFile(const std::string& sPath, const VulkanConfig& stConfig);
