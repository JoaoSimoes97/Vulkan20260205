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
    
    /* --- Level --- */
    std::string sLevelPath = "";  // Set via command-line argument (required)

    /* --- Swapchain --- */
    /** Exact swapchain image count (e.g. 2 = double, 3 = triple buffering). Driver must return this count or app fails. */
    uint32_t lImageCount = static_cast<uint32_t>(3);
    /** Max frames in flight (e.g. 2). Must be at least 1; typically 2. */
    uint32_t lMaxFramesInFlight = static_cast<uint32_t>(2);
    VkPresentModeKHR ePresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;  /* no vsync; use "fifo" for vsync */
    /* Preferred surface format (e.g. "B8G8R8A8_SRGB"). Empty = driver default. */
    std::string sPreferredFormat = "B8G8R8A8_SRGB";
    /* Preferred color space (e.g. "SRGB_NONLINEAR"). Empty = driver default. */
    std::string sPreferredColorSpace = "SRGB_NONLINEAR";

    /* --- Camera / projection --- */
    /** Use perspective (true) or orthographic (false) projection for the main view. */
    bool bUsePerspective = true;
    /** Vertical FOV in radians (perspective only). */
    float fCameraFovYRad = 0.8f;
    /** Near plane distance (perspective and ortho). */
    float fCameraNearZ = 0.1f;
    /** Far plane distance (perspective and ortho). */
    float fCameraFarZ = 100.f;
    /** Ortho only: half extent in Y (view volume ±halfExtent). X = halfExtent/aspect. */
    float fOrthoHalfExtent = 8.f;
    /** Ortho only: near Z in view space. */
    float fOrthoNear = -10.f;
    /** Ortho only: far Z in view space. */
    float fOrthoFar = 10.f;
    /** Camera move speed in units per second (WASD / arrows / QE). */
    float fPanSpeed = 8.0f;
    /** Initial camera position (world space). Perspective: use positive Z to start behind the scene. */
    float fInitialCameraX = 0.f;
    float fInitialCameraY = 0.f;
    float fInitialCameraZ = 8.f;

    /* --- Render --- */
    /** If true, cull back faces (VK_CULL_MODE_BACK_BIT); if false, no face culling (see both sides). */
    bool bCullBackFaces = false;
    /** Clear color (RGBA, 0–1). */
    float fClearColorR = 0.1f;
    float fClearColorG = 0.1f;
    float fClearColorB = 0.4f;
    float fClearColorA = 1.f;
    /** Enable GPU-driven frustum culling via compute shader. Runs parallel to CPU for verification. */
    bool bEnableGPUCulling = true;

    /* --- Debug --- */
    /** Show light debug visualization (wireframe spheres/cones for lights). */
    bool bShowLightDebug = true;

    /* --- GPU Resources --- */
    /** Maximum objects per frame in SSBO. Memory = lMaxObjects × 256 × lMaxFramesInFlight. */
    uint32_t lMaxObjects = 4096;
    /** Descriptor cache: max sets per frame. */
    uint32_t lDescCacheMaxSets = 1000;
    /** Descriptor cache: uniform buffer descriptors per frame. */
    uint32_t lDescCacheUniformBuffers = 500;
    /** Descriptor cache: combined image sampler descriptors per frame. */
    uint32_t lDescCacheSamplers = 500;
    /** Descriptor cache: storage buffer descriptors per frame. */
    uint32_t lDescCacheStorageBuffers = 100;

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
