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
#include <algorithm>
#include <cmath>

using json = nlohmann::json;

namespace {

// Validation ranges for config values
struct ConfigLimits {
    // Window
    static constexpr uint32_t kMinWidth = 320;
    static constexpr uint32_t kMaxWidth = 7680;  // 8K
    static constexpr uint32_t kMinHeight = 240;
    static constexpr uint32_t kMaxHeight = 4320; // 8K
    
    // Swapchain
    static constexpr uint32_t kMinImageCount = 2;
    static constexpr uint32_t kMaxImageCount = 8;
    static constexpr uint32_t kMinFramesInFlight = 1;
    static constexpr uint32_t kMaxFramesInFlight = 4;
    
    // Camera
    static constexpr float kMinFov = 0.1f;        // ~6 degrees
    static constexpr float kMaxFov = 3.14159f;    // ~180 degrees
    static constexpr float kMinNearZ = 0.0001f;
    static constexpr float kMaxFarZ = 1000000.0f;
    static constexpr float kMinPanSpeed = 0.1f;
    static constexpr float kMaxPanSpeed = 100.0f;
    
    // GPU resources
    static constexpr uint32_t kMinMaxObjects = 1;
    static constexpr uint32_t kMaxMaxObjects = 10000000;  // 10M
    static constexpr uint32_t kMinDescSets = 1;
    static constexpr uint32_t kMaxDescSets = 100000;
};

bool ValidateAndClamp(uint32_t& value, uint32_t minVal, uint32_t maxVal, const char* fieldName) {
    if (value < minVal || value > maxVal) {
        uint32_t original = value;
        value = std::clamp(value, minVal, maxVal);
        VulkanUtils::LogWarn("Config '{}': {} out of range [{}, {}], clamped to {}",
                             fieldName, original, minVal, maxVal, value);
        return false;
    }
    return true;
}

bool ValidateAndClampFloat(float& value, float minVal, float maxVal, const char* fieldName) {
    if (!std::isfinite(value) || value < minVal || value > maxVal) {
        float original = value;
        value = std::clamp(value, minVal, maxVal);
        VulkanUtils::LogWarn("Config '{}': {} out of range [{:.6f}, {:.6f}], clamped to {:.6f}",
                             fieldName, original, minVal, maxVal, value);
        return false;
    }
    return true;
}

bool ValidateAndClampColor(float& value, const char* fieldName) {
    return ValidateAndClampFloat(value, 0.0f, 1.0f, fieldName);
}

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
        if ((jRender.contains("enable_gpu_culling") == true) && (jRender["enable_gpu_culling"].is_boolean() == true))
            stConfig.bEnableGPUCulling = jRender["enable_gpu_culling"].get<bool>();
    }
    if (jRoot.contains("debug") == true) {
        const json& jDebug = jRoot["debug"];
        if ((jDebug.contains("show_light_debug") == true) && (jDebug["show_light_debug"].is_boolean() == true))
            stConfig.bShowLightDebug = jDebug["show_light_debug"].get<bool>();
    }
    if (jRoot.contains("gpu_resources") == true) {
        const json& jGpu = jRoot["gpu_resources"];
        if ((jGpu.contains("max_objects") == true) && (jGpu["max_objects"].is_number_unsigned() == true))
            stConfig.lMaxObjects = jGpu["max_objects"].get<uint32_t>();
        if ((jGpu.contains("desc_cache_max_sets") == true) && (jGpu["desc_cache_max_sets"].is_number_unsigned() == true))
            stConfig.lDescCacheMaxSets = jGpu["desc_cache_max_sets"].get<uint32_t>();
        if ((jGpu.contains("desc_cache_uniform_buffers") == true) && (jGpu["desc_cache_uniform_buffers"].is_number_unsigned() == true))
            stConfig.lDescCacheUniformBuffers = jGpu["desc_cache_uniform_buffers"].get<uint32_t>();
        if ((jGpu.contains("desc_cache_samplers") == true) && (jGpu["desc_cache_samplers"].is_number_unsigned() == true))
            stConfig.lDescCacheSamplers = jGpu["desc_cache_samplers"].get<uint32_t>();
        if ((jGpu.contains("desc_cache_storage_buffers") == true) && (jGpu["desc_cache_storage_buffers"].is_number_unsigned() == true))
            stConfig.lDescCacheStorageBuffers = jGpu["desc_cache_storage_buffers"].get<uint32_t>();
    }
    if (jRoot.contains("editor") == true) {
        const json& jEditor = jRoot["editor"];
        if ((jEditor.contains("layout_file") == true) && (jEditor["layout_file"].is_string() == true))
            stConfig.sEditorLayoutPath = jEditor["layout_file"].get<std::string>();
    }
    /* validation_layers not loaded from config — dev/debug only, set from build type or env. */
}

} // namespace

bool ValidateConfig(VulkanConfig& stConfig) {
    bool bAllValid = true;
    
    // Window validation
    bAllValid &= ValidateAndClamp(stConfig.lWidth, ConfigLimits::kMinWidth, ConfigLimits::kMaxWidth, "window.width");
    bAllValid &= ValidateAndClamp(stConfig.lHeight, ConfigLimits::kMinHeight, ConfigLimits::kMaxHeight, "window.height");
    
    // Swapchain validation
    bAllValid &= ValidateAndClamp(stConfig.lImageCount, ConfigLimits::kMinImageCount, ConfigLimits::kMaxImageCount, "swapchain.image_count");
    bAllValid &= ValidateAndClamp(stConfig.lMaxFramesInFlight, ConfigLimits::kMinFramesInFlight, ConfigLimits::kMaxFramesInFlight, "swapchain.max_frames_in_flight");
    
    // Camera validation
    bAllValid &= ValidateAndClampFloat(stConfig.fCameraFovYRad, ConfigLimits::kMinFov, ConfigLimits::kMaxFov, "camera.fov_y_rad");
    bAllValid &= ValidateAndClampFloat(stConfig.fCameraNearZ, ConfigLimits::kMinNearZ, stConfig.fCameraFarZ, "camera.near_z");
    bAllValid &= ValidateAndClampFloat(stConfig.fCameraFarZ, stConfig.fCameraNearZ, ConfigLimits::kMaxFarZ, "camera.far_z");
    bAllValid &= ValidateAndClampFloat(stConfig.fPanSpeed, ConfigLimits::kMinPanSpeed, ConfigLimits::kMaxPanSpeed, "camera.pan_speed");
    
    // Ortho validation: ortho_near < ortho_far
    if (stConfig.fOrthoNear >= stConfig.fOrthoFar) {
        VulkanUtils::LogWarn("Config 'camera.ortho_near' ({}) >= 'camera.ortho_far' ({}), swapping",
                             stConfig.fOrthoNear, stConfig.fOrthoFar);
        std::swap(stConfig.fOrthoNear, stConfig.fOrthoFar);
        bAllValid = false;
    }
    bAllValid &= ValidateAndClampFloat(stConfig.fOrthoHalfExtent, 0.001f, 10000.0f, "camera.ortho_half_extent");
    
    // Render validation (clear colors 0-1)
    bAllValid &= ValidateAndClampColor(stConfig.fClearColorR, "render.clear_color_r");
    bAllValid &= ValidateAndClampColor(stConfig.fClearColorG, "render.clear_color_g");
    bAllValid &= ValidateAndClampColor(stConfig.fClearColorB, "render.clear_color_b");
    bAllValid &= ValidateAndClampColor(stConfig.fClearColorA, "render.clear_color_a");
    
    // GPU resources validation
    bAllValid &= ValidateAndClamp(stConfig.lMaxObjects, ConfigLimits::kMinMaxObjects, ConfigLimits::kMaxMaxObjects, "gpu_resources.max_objects");
    bAllValid &= ValidateAndClamp(stConfig.lDescCacheMaxSets, ConfigLimits::kMinDescSets, ConfigLimits::kMaxDescSets, "gpu_resources.desc_cache_max_sets");
    bAllValid &= ValidateAndClamp(stConfig.lDescCacheUniformBuffers, ConfigLimits::kMinDescSets, ConfigLimits::kMaxDescSets, "gpu_resources.desc_cache_uniform_buffers");
    bAllValid &= ValidateAndClamp(stConfig.lDescCacheSamplers, ConfigLimits::kMinDescSets, ConfigLimits::kMaxDescSets, "gpu_resources.desc_cache_samplers");
    bAllValid &= ValidateAndClamp(stConfig.lDescCacheStorageBuffers, ConfigLimits::kMinDescSets, ConfigLimits::kMaxDescSets, "gpu_resources.desc_cache_storage_buffers");
    
    return bAllValid;
}

bool ValidateConfigGPULimits(VulkanConfig& stConfig, const VkPhysicalDeviceLimits& limits) {
    bool bAllValid = true;
    
    // Calculate required buffer size for ObjectData SSBO (per frame)
    // ObjectData is typically 256 bytes (see vulkan_types.h)
    constexpr VkDeviceSize kObjectDataSize = 256;  // sizeof(ObjectData)
    VkDeviceSize requiredStorageSize = static_cast<VkDeviceSize>(stConfig.lMaxObjects) * kObjectDataSize;
    
    // Check against maxStorageBufferRange
    if (requiredStorageSize > limits.maxStorageBufferRange) {
        uint32_t maxAllowed = static_cast<uint32_t>(limits.maxStorageBufferRange / kObjectDataSize);
        VulkanUtils::LogWarn("gpu_resources.max_objects {} exceeds GPU maxStorageBufferRange ({} bytes = {} objects), clamping to {}",
                             stConfig.lMaxObjects, limits.maxStorageBufferRange, maxAllowed, maxAllowed);
        stConfig.lMaxObjects = maxAllowed;
        bAllValid = false;
    }
    
    // Check descriptor pool sizes against GPU limits
    if (stConfig.lDescCacheMaxSets > limits.maxDescriptorSetUniformBuffers) {
        VulkanUtils::LogWarn("gpu_resources.desc_cache_max_sets {} > GPU maxDescriptorSetUniformBuffers {}, clamping",
                             stConfig.lDescCacheMaxSets, limits.maxDescriptorSetUniformBuffers);
        stConfig.lDescCacheMaxSets = limits.maxDescriptorSetUniformBuffers;
        bAllValid = false;
    }
    
    if (stConfig.lDescCacheUniformBuffers > limits.maxDescriptorSetUniformBuffers) {
        VulkanUtils::LogWarn("gpu_resources.desc_cache_uniform_buffers {} > GPU maxDescriptorSetUniformBuffers {}, clamping",
                             stConfig.lDescCacheUniformBuffers, limits.maxDescriptorSetUniformBuffers);
        stConfig.lDescCacheUniformBuffers = limits.maxDescriptorSetUniformBuffers;
        bAllValid = false;
    }
    
    if (stConfig.lDescCacheSamplers > limits.maxDescriptorSetSamplers) {
        VulkanUtils::LogWarn("gpu_resources.desc_cache_samplers {} > GPU maxDescriptorSetSamplers {}, clamping",
                             stConfig.lDescCacheSamplers, limits.maxDescriptorSetSamplers);
        stConfig.lDescCacheSamplers = limits.maxDescriptorSetSamplers;
        bAllValid = false;
    }
    
    if (stConfig.lDescCacheStorageBuffers > limits.maxDescriptorSetStorageBuffers) {
        VulkanUtils::LogWarn("gpu_resources.desc_cache_storage_buffers {} > GPU maxDescriptorSetStorageBuffers {}, clamping",
                             stConfig.lDescCacheStorageBuffers, limits.maxDescriptorSetStorageBuffers);
        stConfig.lDescCacheStorageBuffers = limits.maxDescriptorSetStorageBuffers;
        bAllValid = false;
    }
    
    // Log GPU limits for debugging
    VulkanUtils::LogInfo("GPU limits: maxStorageBufferRange={}, maxDescriptorSetSamplers={}, maxDescriptorSetStorageBuffers={}",
                         limits.maxStorageBufferRange, limits.maxDescriptorSetSamplers, limits.maxDescriptorSetStorageBuffers);
    
    return bAllValid;
}

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
    stCfg.fPanSpeed = 8.0f;
    stCfg.fInitialCameraX = 0.f;
    stCfg.fInitialCameraY = 0.f;
    stCfg.fInitialCameraZ = 8.f;
    stCfg.bCullBackFaces = false;
    stCfg.fClearColorR = 0.1f;
    stCfg.fClearColorG = 0.1f;
    stCfg.fClearColorB = 0.4f;
    stCfg.fClearColorA = 1.f;
    stCfg.bEnableGPUCulling = true;
    stCfg.bShowLightDebug = true;
    stCfg.lMaxObjects = 100000;  // 100k objects - uses ~400MB for GPU culling buffers
    stCfg.lDescCacheMaxSets = 1000;
    stCfg.lDescCacheUniformBuffers = 500;
    stCfg.lDescCacheSamplers = 500;
    stCfg.lDescCacheStorageBuffers = 100;
    stCfg.sEditorLayoutPath = "config/imgui_layout.ini";
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
        ValidateConfig(stResult);  // Validate before saving
        SaveConfigToFile(sUserPath_ic, stResult);
        VulkanUtils::LogInfo("User config not found at \"{}\"; created from default. Edit the file and restart to change settings.", sUserPath_ic);
        return stResult;
    }
    
    bool bNeedRewrite = false;
    try {
        json jUser = json::parse(stmUser);
        stmUser.close();
        
        // Check for missing required sections (if any section is missing, we'll rewrite)
        const char* requiredSections[] = {"window", "swapchain", "camera", "render", "debug", "gpu_resources", "editor"};
        for (const char* section : requiredSections) {
            if (!jUser.contains(section)) {
                VulkanUtils::LogWarn("Config missing section '{}', will regenerate config file with defaults", section);
                bNeedRewrite = true;
            }
        }
        
        ApplyJsonToConfig(jUser, stResult);
    } catch (const json::exception& e) {
        VulkanUtils::LogWarn("Failed to parse user config \"{}\": {}. Using defaults.", sUserPath_ic, e.what());
        bNeedRewrite = true;
    }
    
    ValidateConfig(stResult);  // Validate merged config
    
    // If config was missing fields, regenerate it with all fields populated
    if (bNeedRewrite) {
        SaveConfigToFile(sUserPath_ic, stResult);
        VulkanUtils::LogInfo("Config file regenerated with all fields: {}", sUserPath_ic);
    }
    
    return stResult;
}

VulkanConfig LoadConfigFromFile(const std::string& sPath_ic) {
    VulkanConfig stConfig;
    std::ifstream stmIn(sPath_ic);
    if (stmIn.is_open() == false) {
        VulkanUtils::LogWarn("Config file not found: {}", sPath_ic);
        return stConfig;
    }
    try {
        json jRoot = json::parse(stmIn);
        ApplyJsonToConfig(jRoot, stConfig);
    } catch (const json::exception& e) {
        VulkanUtils::LogWarn("Failed to parse config \"{}\": {}. Using defaults.", sPath_ic, e.what());
    }
    ValidateConfig(stConfig);
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
            { "clear_color_a", stConfig_ic.fClearColorA },
            { "enable_gpu_culling", stConfig_ic.bEnableGPUCulling }
        }},
        { "debug", {
            { "show_light_debug", stConfig_ic.bShowLightDebug }
        }},
        { "gpu_resources", {
            { "max_objects", stConfig_ic.lMaxObjects },
            { "desc_cache_max_sets", stConfig_ic.lDescCacheMaxSets },
            { "desc_cache_uniform_buffers", stConfig_ic.lDescCacheUniformBuffers },
            { "desc_cache_samplers", stConfig_ic.lDescCacheSamplers },
            { "desc_cache_storage_buffers", stConfig_ic.lDescCacheStorageBuffers }
        }},
        { "editor", {
            { "layout_file", stConfig_ic.sEditorLayoutPath }
        }}
    };
    std::ofstream stmOut(sPath_ic);
    if (stmOut.is_open() == true)
        stmOut << jRoot.dump(2);
}
