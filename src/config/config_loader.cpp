#include "config_loader.h"
#include "vulkan_config.h"
#include "vulkan_utils.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

void ApplyJsonToConfig(const json& jRoot, VulkanConfig& stConfig) {
    if (jRoot.contains("window") == true) {
        const json& jWindow = jRoot["window"];
        if ((jWindow.contains("width") == true) && (jWindow["width"].is_number_unsigned() == true))
            stConfig.lWidth = jWindow["width"].get<uint32_t>();
        if ((jWindow.contains("height") == true) && (jWindow["height"].is_number_unsigned() == true))
            stConfig.lHeight = jWindow["height"].get<uint32_t>();
        if ((jWindow.contains("fullscreen") == true) && (jWindow["fullscreen"].is_boolean() == true))
            stConfig.bFullscreen = jWindow["fullscreen"].get<bool>();
        if ((jWindow.contains("title") == true) && (jWindow["title"].is_string() == true))
            stConfig.sWindowTitle = jWindow["title"].get<std::string>();
    }
    if (jRoot.contains("swapchain") == true) {
        const json& jSwapchain = jRoot["swapchain"];
        if ((jSwapchain.contains("present_mode") == true) && (jSwapchain["present_mode"].is_string() == true))
            stConfig.ePresentMode = PresentModeFromString(jSwapchain["present_mode"].get<std::string>());
        if ((jSwapchain.contains("preferred_format") == true) && (jSwapchain["preferred_format"].is_string() == true))
            stConfig.sPreferredFormat = jSwapchain["preferred_format"].get<std::string>();
        if ((jSwapchain.contains("preferred_color_space") == true) && (jSwapchain["preferred_color_space"].is_string() == true))
            stConfig.sPreferredColorSpace = jSwapchain["preferred_color_space"].get<std::string>();
    }
    /* validation_layers not loaded from config — dev/debug only, set from build type or env. */
}

} // namespace

VulkanConfig GetDefaultConfig() {
    VulkanConfig stCfg;
    stCfg.lWidth = static_cast<uint32_t>(800);
    stCfg.lHeight = static_cast<uint32_t>(600);
    stCfg.bFullscreen = static_cast<bool>(false);
    stCfg.sWindowTitle = "Vulkan App";
    stCfg.ePresentMode = VK_PRESENT_MODE_FIFO_KHR;
    stCfg.sPreferredFormat = "B8G8R8A8_SRGB";
    stCfg.sPreferredColorSpace = "SRGB_NONLINEAR";
    stCfg.bValidationLayers = static_cast<bool>(false);
    stCfg.bSwapchainDirty = static_cast<bool>(false);
    return stCfg;
}

void EnsureDefaultConfigFile(const std::string& sDefaultPath) {
    std::ifstream stmIn(sDefaultPath);
    if (stmIn.is_open() == true) {
        stmIn.close();
        return;
    }
    VulkanConfig stDefault = GetDefaultConfig();
    SaveConfigToFile(sDefaultPath, stDefault);
    VulkanUtils::LogInfo("Default config not found at \"{}\"; created. This file is not overwritten by the app.", sDefaultPath);
}

VulkanConfig LoadConfigFromFileOrCreate(const std::string& sUserPath, const std::string& sDefaultPath) {
    EnsureDefaultConfigFile(sDefaultPath);
    VulkanConfig stResult = LoadConfigFromFile(sDefaultPath);

    std::ifstream stmUser(sUserPath);
    if (stmUser.is_open() == false) {
        SaveConfigToFile(sUserPath, stResult);
        VulkanUtils::LogInfo("User config not found at \"{}\"; created from default. Edit the file and restart to change settings.", sUserPath);
        return stResult;
    }
    try {
        json jUser = json::parse(stmUser);
        ApplyJsonToConfig(jUser, stResult);
    } catch (const json::exception&) {
        /* On parse error, keep default-based result (stResult already has default). */
    }
    return stResult;
}

VulkanConfig LoadConfigFromFile(const std::string& sPath) {
    VulkanConfig stConfig;
    std::ifstream stmIn(sPath);
    if (stmIn.is_open() == false)
        return stConfig;
    try {
        json jRoot = json::parse(stmIn);
        ApplyJsonToConfig(jRoot, stConfig);
    } catch (const json::exception&) {
        /* Return defaults on parse error */
    }
    return stConfig;
}

void SaveConfigToFile(const std::string& sPath, const VulkanConfig& stConfig) {
    std::filesystem::path p(sPath);
    std::filesystem::path parent = p.parent_path();
    if (parent.empty() == false)
        std::filesystem::create_directories(parent);
    /* Build JSON tree: window and swapchain sections. */
    json jRoot = {
        { "window", {
            { "width", stConfig.lWidth },
            { "height", stConfig.lHeight },
            { "fullscreen", stConfig.bFullscreen },
            { "title", stConfig.sWindowTitle }
        }},
        { "swapchain", {
            { "present_mode", PresentModeToString(stConfig.ePresentMode) },
            { "preferred_format", stConfig.sPreferredFormat.empty() == true ? "B8G8R8A8_SRGB" : stConfig.sPreferredFormat },
            { "preferred_color_space", stConfig.sPreferredColorSpace.empty() == true ? "SRGB_NONLINEAR" : stConfig.sPreferredColorSpace }
        }}
    };
    std::ofstream stmOut(sPath);
    if (stmOut.is_open() == true)
        stmOut << jRoot.dump(2);
}
