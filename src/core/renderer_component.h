/*
 * RendererComponent — Visual representation of a GameObject.
 * Holds references to mesh and material, plus rendering state.
 */
#pragma once

#include "component.h"
#include <memory>

class MeshHandle;
struct MaterialHandle;
class TextureHandle;

/**
 * Material properties for PBR rendering.
 * Stored per-renderer for GPU upload to material SSBO.
 */
struct MaterialProperties {
    /** Base color (RGBA). From glTF baseColorFactor. */
    float baseColor[4] = {1.f, 1.f, 1.f, 1.f};

    /** Emissive color (RGB) + strength (A). */
    float emissive[4] = {0.f, 0.f, 0.f, 1.f};

    /** Metallic factor (0-1). From glTF pbrMetallicRoughness.metallicFactor. */
    float metallic = 0.f;

    /** Roughness factor (0-1). From glTF pbrMetallicRoughness.roughnessFactor. */
    float roughness = 1.f;

    /** Normal map scale (0-1). Future use. */
    float normalScale = 1.f;

    /** Ambient occlusion strength (0-1). Future use. */
    float occlusionStrength = 1.f;
};

/**
 * Render layer for sorting and culling.
 */
enum class RenderLayer : uint32_t {
    Default = 0,
    Background = 1,
    Foreground = 2,
    UI = 3,
    Debug = 4,
    COUNT
};

/**
 * Renderer component data.
 * Describes how to render a GameObject.
 */
struct RendererComponent {
    /** Mesh geometry. Resolved to VkBuffer at draw time. */
    std::shared_ptr<MeshHandle> mesh;

    /** Material (pipeline key, layout). Resolved to VkPipeline at draw time. */
    std::shared_ptr<MaterialHandle> material;

    /** Optional per-object texture. nullptr = use default white. */
    std::shared_ptr<TextureHandle> texture;

    /** PBR textures (nullptr = use factors only or default). */
    std::shared_ptr<TextureHandle> pMetallicRoughnessTexture;
    std::shared_ptr<TextureHandle> pEmissiveTexture;
    std::shared_ptr<TextureHandle> pNormalTexture;
    std::shared_ptr<TextureHandle> pOcclusionTexture;

    /** Material properties for PBR shading. */
    MaterialProperties matProps;

    /** Emissive as light: create a point light for this object. */
    bool emitsLight = false;
    float emissiveLightRadius = 15.f;
    float emissiveLightIntensity = 5.f;

    /** Instance tier for batching (see object.h InstanceTier). */
    uint8_t instanceTier = 0;  // 0 = Static

    /** Render layer for sorting. */
    RenderLayer layer = RenderLayer::Default;

    /** Cast shadows (future). */
    bool bCastShadow = true;

    /** Receive shadows (future). */
    bool bReceiveShadow = true;

    /** Visible flag. Set false to skip rendering without removing component. */
    bool bVisible = true;

    /** Index of the owning GameObject in the scene. Used for SSBO indexing. */
    uint32_t gameObjectIndex = 0;
};

