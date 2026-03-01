/*
 * ViewportConfig — Configuration for a single viewport (camera + rendering settings).
 * Each viewport can have its own camera, render mode, and post-processing settings.
 */
#pragma once

#include <cstdint>
#include <string>

/**
 * Simple 2D vector for viewport positions/sizes.
 */
struct ViewportVec2 {
    float x = 0.0f;
    float y = 0.0f;
};

/**
 * Simple color for viewport clear color.
 */
struct ViewportColor {
    float r = 0.1f;
    float g = 0.1f;
    float b = 0.1f;
    float a = 1.0f;
};

/**
 * Render mode for a viewport.
 */
enum class ViewportRenderMode : uint8_t {
    Solid = 0,      // Standard PBR rendering
    Wireframe,      // Wireframe overlay
    Unlit,          // No lighting calculations
    Normals,        // Visualize normals as colors
    Depth,          // Visualize depth buffer
    UV,             // Visualize UV coordinates
    COUNT
};

/**
 * Post-processing flags for a viewport.
 */
enum class ViewportPostProcess : uint32_t {
    None        = 0,
    ToneMapping = 1 << 0,
    Bloom       = 1 << 1,
    FXAA        = 1 << 2,
    Vignette    = 1 << 3,
    ColorGrading = 1 << 4,
    All         = 0xFFFFFFFF
};

inline ViewportPostProcess operator|(ViewportPostProcess a, ViewportPostProcess b) {
    return static_cast<ViewportPostProcess>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline ViewportPostProcess operator&(ViewportPostProcess a, ViewportPostProcess b) {
    return static_cast<ViewportPostProcess>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool HasFlag(ViewportPostProcess flags, ViewportPostProcess flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

/**
 * Viewport configuration — all settings for a single viewport.
 */
struct ViewportConfig {
    /** Unique identifier for this viewport. */
    uint32_t id = 0;
    
    /** Human-readable name (shown in UI). */
    std::string name = "Viewport";
    
    /** Is this the main viewport (renders to swapchain)? */
    bool bIsMainViewport = false;
    
    /** ID of the camera GameObject to use. UINT32_MAX = main/editor camera. */
    uint32_t cameraGameObjectId = UINT32_MAX;
    
    /** Render mode for this viewport. */
    ViewportRenderMode renderMode = ViewportRenderMode::Solid;
    
    /** Post-processing flags. */
    ViewportPostProcess postProcess = ViewportPostProcess::ToneMapping;
    
    /** Is this viewport visible? */
    bool bVisible = true;
    
    /** Is this viewport detached into its own window? */
    bool bDetached = false;
    
    /** Picture-in-picture position (top-left corner). */
    ViewportVec2 pipPosition = {0.7f, 0.0f};
    
    /** Picture-in-picture size. */
    ViewportVec2 pipSize = {320.0f, 180.0f};
    
    /** Clear color for this viewport. */
    ViewportColor clearColor = {0.1f, 0.1f, 0.1f, 1.0f};
    
    /** Show gizmos in this viewport? */
    bool bShowGizmos = false;
    
    /** Show grid in this viewport? */
    bool bShowGrid = false;
    
    /** Show light debug visualization in this viewport? */
    bool bShowLightDebug = false;
    
    /** Field of view override (0 = use camera's default). */
    float fovOverride = 0.0f;
    
    /** Near/far plane overrides (0 = use camera's default). */
    float nearPlaneOverride = 0.0f;
    float farPlaneOverride = 0.0f;
};

/**
 * Get render mode name as string.
 */
inline const char* GetRenderModeName(ViewportRenderMode mode) {
    switch (mode) {
        case ViewportRenderMode::Solid: return "Solid";
        case ViewportRenderMode::Wireframe: return "Wireframe";
        case ViewportRenderMode::Unlit: return "Unlit";
        case ViewportRenderMode::Normals: return "Normals";
        case ViewportRenderMode::Depth: return "Depth";
        case ViewportRenderMode::UV: return "UV";
        default: return "Unknown";
    }
}
