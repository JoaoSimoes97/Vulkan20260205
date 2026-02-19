/*
 * CameraComponent — Camera attachment for GameObjects.
 * Defines view frustum, projection, and rendering settings.
 * 
 * Multiple cameras supported for:
 * - Main gameplay camera
 * - Editor scene camera
 * - Minimaps, security cameras, etc.
 * - Multi-viewport rendering
 */
#pragma once

#include "component.h"
#include <cstdint>

/**
 * Camera projection type.
 */
enum class ProjectionType : uint32_t {
    Perspective = 0,
    Orthographic,
    COUNT
};

/**
 * Camera clear flags.
 */
enum class CameraClearFlags : uint32_t {
    Skybox = 0,         /** Clear with skybox. */
    SolidColor,         /** Clear with solid color. */
    DepthOnly,          /** Clear depth only. */
    Nothing,            /** Don't clear (for overlay cameras). */
    COUNT
};

/**
 * CameraComponent — Defines a viewpoint and projection.
 */
struct CameraComponent {
    /** Projection type. */
    ProjectionType projection = ProjectionType::Perspective;
    
    /** Field of view (radians) for perspective. */
    float fov = 1.0472f;  /** ~60 degrees */
    
    /** Orthographic size (half-height in world units). */
    float orthoSize = 5.0f;
    
    /** Near clip plane. */
    float nearClip = 0.1f;
    
    /** Far clip plane. */
    float farClip = 1000.0f;
    
    /** Aspect ratio override (0 = use viewport). */
    float aspectRatio = 0.0f;
    
    /** Clear flags. */
    CameraClearFlags clearFlags = CameraClearFlags::SolidColor;
    
    /** Clear color (RGBA). */
    float clearColor[4] = {0.1f, 0.1f, 0.1f, 1.0f};
    
    /** Viewport rectangle (normalized 0-1). */
    float viewportX = 0.0f;
    float viewportY = 0.0f;
    float viewportWidth = 1.0f;
    float viewportHeight = 1.0f;
    
    /** Render priority (lower = renders first). */
    int32_t depth = 0;
    
    /** Culling mask (which layers this camera renders). */
    uint32_t cullingMask = 0xFFFFFFFF;
    
    /** Is this the main camera? */
    bool bIsMain = false;
    
    /** Index of the owning GameObject. */
    uint32_t gameObjectIndex = 0;
    
    /** Cached view matrix (column-major). Updated by CameraSystem. */
    float viewMatrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    
    /** Cached projection matrix (column-major). Updated on parameter change. */
    float projectionMatrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    
    /** Cached view-projection matrix. */
    float viewProjectionMatrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
};


