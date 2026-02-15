/*
 * Config loader — default.json (immutable) + config.json (user). Load merges user over default.
 * See config_loader.h and docs/architecture.md for JSON layout.
 */
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
        if ((jSwapchain.contains("image_count") == true) && (jSwapchain["image_count"].is_number_unsigned() == true))
            stConfig.lImageCount = jSwapchain["image_count"].get<uint32_t>();
        if ((jSwapchain.contains("max_frames_in_flight") == true) && (jSwapchain["max_frames_in_flight"].is_number_unsigned() == true))
            stConfig.lMaxFramesInFlight = jSwapchain["max_frames_in_flight"].get<uint32_t>();
        if ((jSwapchain.contains("present_mode") == true) && (jSwapchain["present_mode"].is_string() == true))
            stConfig.ePresentMode = PresentModeFromString(jSwapchain["present_mode"].get<std::string>());
        if ((jSwapchain.contains("preferred_format") == true) && (jSwapchain["preferred_format"].is_string() == true))
            stConfig.sPreferredFormat = jSwapchain["preferred_format"].get<std::string>();
        if ((jSwapchain.contains("preferred_color_space") == true) && (jSwapchain["preferred_color_space"].is_string() == true))
            stConfig.sPreferredColorSpace = jSwapchain["preferred_color_space"].get<std::string>();
    }
    if (jRoot.contains("camera") == true) {
        const json& jCam = jRoot["camera"];
        if ((jCam.contains("use_perspective") == true) && (jCam["use_perspective"].is_boolean() == true))
            stConfig.bUsePerspective = jCam["use_perspective"].get<bool>();
        if ((jCam.contains("fov_y_rad") == true) && (jCam["fov_y_rad"].is_number() == true))
            stConfig.fCameraFovYRad = static_cast<float>(jCam["fov_y_rad"].get<double>());
        if ((jCam.contains("near_z") == true) && (jCam["near_z"].is_number() == true))
            stConfig.fCameraNearZ = static_cast<float>(jCam["near_z"].get<double>());
        if ((jCam.contains("far_z") == true) && (jCam["far_z"].is_number() == true))
            stConfig.fCameraFarZ = static_cast<float>(jCam["far_z"].get<double>());
        if ((jCam.contains("ortho_half_extent") == true) && (jCam["ortho_half_extent"].is_number() == true))
            stConfig.fOrthoHalfExtent = static_cast<float>(jCam["ortho_half_extent"].get<double>());
        if ((jCam.contains("ortho_near") == true) && (jCam["ortho_near"].is_number() == true))
            stConfig.fOrthoNear = static_cast<float>(jCam["ortho_near"].get<double>());
        if ((jCam.contains("ortho_far") == true) && (jCam["ortho_far"].is_number() == true))
            stConfig.fOrthoFar = static_cast<float>(jCam["ortho_far"].get<double>());
        if ((jCam.contains("pan_speed") == true) && (jCam["pan_speed"].is_number() == true))
            stConfig.fPanSpeed = static_cast<float>(jCam["pan_speed"].get<double>());
        if ((jCam.contains("initial_camera_x") == true) && (jCam["initial_camera_x"].is_number() == true))
            stConfig.fInitialCameraX = static_cast<float>(jCam["initial_camera_x"].get<double>());
        if ((jCam.contains("initial_camera_y") == true) && (jCam["initial_camera_y"].is_number() == true))
            stConfig.fInitialCameraY = static_cast<float>(jCam["initial_camera_y"].get<double>());
        if ((jCam.contains("initial_camera_z") == true) && (jCam["initial_camera_z"].is_number() == true))
            stConfig.fInitialCameraZ = static_cast<float>(jCam["initial_camera_z"].get<double>());
    }
    if (jRoot.contains("render") == true) {
        const json& jRender = jRoot["render"];
        if ((jRender.contains("cull_back_faces") == true) && (jRender["cull_back_faces"].is_boolean() == true))
            stConfig.bCullBackFaces = jRender["cull_back_faces"].get<bool>();
        if ((jRender.contains("clear_color_r") == true) && (jRender["clear_color_r"].is_number() == true))
            stConfig.fClearColorR = static_cast<float>(jRender["clear_color_r"].get<double>());
        if ((jRender.contains("clear_color_g") == true) && (jRender["clear_color_g"].is_number() == true))
            stConfig.fClearColorG = static_cast<float>(jRender["clear_color_g"].get<double>());
        if ((jRender.contains("clear_color_b") == true) && (jRender["clear_color_b"].is_number() == true))
            stConfig.fClearColorB = static_cast<float>(jRender["clear_color_b"].get<double>());
        if ((jRender.contains("clear_color_a") == true) && (jRender["clear_color_a"].is_number() == true))
            stConfig.fClearColorA = static_cast<float>(jRender["clear_color_a"].get<double>());
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
    stCfg.lImageCount = static_cast<uint32_t>(3);
    stCfg.lMaxFramesInFlight = static_cast<uint32_t>(2);
    stCfg.ePresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;  /* no vsync; use VK_PRESENT_MODE_FIFO_KHR for vsync */
    stCfg.sPreferredFormat = "B8G8R8A8_SRGB";
    stCfg.sPreferredColorSpace = "SRGB_NONLINEAR";
    stCfg.bUsePerspective = true;
    stCfg.fCameraFovYRad = 0.8f;
    stCfg.fCameraNearZ = 0.1f;
    stCfg.fCameraFarZ = 100.f;
    stCfg.fOrthoHalfExtent = 8.f;
    stCfg.fOrthoNear = -10.f;
    stCfg.fOrthoFar = 10.f;
    stCfg.fPanSpeed = 0.012f;
    stCfg.fInitialCameraX = 0.f;
    stCfg.fInitialCameraY = 0.f;
    stCfg.fInitialCameraZ = 8.f;
    stCfg.bCullBackFaces = false;
    stCfg.fClearColorR = 0.1f;
    stCfg.fClearColorG = 0.1f;
    stCfg.fClearColorB = 0.4f;
    stCfg.fClearColorA = 1.f;
    stCfg.bValidationLayers = static_cast<bool>(false);
    stCfg.bSwapchainDirty = static_cast<bool>(false);
    return stCfg;
}

void EnsureDefaultConfigFile(const std::string& sDefaultPath_ic) {
    std::ifstream stmIn(sDefaultPath_ic);
    if (stmIn.is_open() == true) {
        stmIn.close();
        return;
    }
    VulkanConfig stDefault = GetDefaultConfig();
    SaveConfigToFile(sDefaultPath_ic, stDefault);
    VulkanUtils::LogInfo("Default config not found at \"{}\"; created. This file is not overwritten by the app.", sDefaultPath_ic);
}

VulkanConfig LoadConfigFromFileOrCreate(const std::string& sUserPath_ic, const std::string& sDefaultPath_ic) {
    EnsureDefaultConfigFile(sDefaultPath_ic);
    VulkanConfig stResult = LoadConfigFromFile(sDefaultPath_ic);

    std::ifstream stmUser(sUserPath_ic);
    if (stmUser.is_open() == false) {
        SaveConfigToFile(sUserPath_ic, stResult);
        VulkanUtils::LogInfo("User config not found at \"{}\"; created from default. Edit the file and restart to change settings.", sUserPath_ic);
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

VulkanConfig LoadConfigFromFile(const std::string& sPath_ic) {
    VulkanConfig stConfig;
    std::ifstream stmIn(sPath_ic);
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

void SaveConfigToFile(const std::string& sPath_ic, const VulkanConfig& stConfig_ic) {
    std::filesystem::path p(sPath_ic);
    std::filesystem::path parent = p.parent_path();
    if (parent.empty() == false)
        std::filesystem::create_directories(parent);
    /* Build JSON tree: window and swapchain sections. */
    json jRoot = {
        { "window", {
            { "width", stConfig_ic.lWidth },
            { "height", stConfig_ic.lHeight },
            { "fullscreen", stConfig_ic.bFullscreen },
            { "title", stConfig_ic.sWindowTitle }
        }},
        { "swapchain", {
            { "image_count", stConfig_ic.lImageCount },
            { "max_frames_in_flight", stConfig_ic.lMaxFramesInFlight },
            { "present_mode", PresentModeToString(stConfig_ic.ePresentMode) },
            { "preferred_format", stConfig_ic.sPreferredFormat.empty() == true ? "B8G8R8A8_SRGB" : stConfig_ic.sPreferredFormat },
            { "preferred_color_space", stConfig_ic.sPreferredColorSpace.empty() == true ? "SRGB_NONLINEAR" : stConfig_ic.sPreferredColorSpace }
        }},
        { "camera", {
            { "use_perspective", stConfig_ic.bUsePerspective },
            { "fov_y_rad", stConfig_ic.fCameraFovYRad },
            { "near_z", stConfig_ic.fCameraNearZ },
            { "far_z", stConfig_ic.fCameraFarZ },
            { "ortho_half_extent", stConfig_ic.fOrthoHalfExtent },
            { "ortho_near", stConfig_ic.fOrthoNear },
            { "ortho_far", stConfig_ic.fOrthoFar },
            { "pan_speed", stConfig_ic.fPanSpeed },
            { "initial_camera_x", stConfig_ic.fInitialCameraX },
            { "initial_camera_y", stConfig_ic.fInitialCameraY },
            { "initial_camera_z", stConfig_ic.fInitialCameraZ }
        }},
        { "render", {
            { "cull_back_faces", stConfig_ic.bCullBackFaces },
            { "clear_color_r", stConfig_ic.fClearColorR },
            { "clear_color_g", stConfig_ic.fClearColorG },
            { "clear_color_b", stConfig_ic.fClearColorB },
            { "clear_color_a", stConfig_ic.fClearColorA }
        }}
    };
    std::ofstream stmOut(sPath_ic);
    if (stmOut.is_open() == true)
        stmOut << jRoot.dump(2);
}
