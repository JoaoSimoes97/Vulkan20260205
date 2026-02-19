/*
 * LightComponent — Light source for scene illumination.
 * Supports directional, point, and spot lights.
 */
#pragma once

#include <cstdint>
#include <cstring>

/**
 * Light type enumeration.
 */
enum class LightType : uint32_t {
    Directional = 0,    /** Sun-like light, no position, only direction. */
    Point = 1,          /** Omnidirectional light with falloff. */
    Spot = 2,           /** Cone-shaped light with direction and falloff. */
    Area = 3,           /** Rectangular emitter (Phase 4+). */
    COUNT
};

/**
 * Light component data.
 * Position/direction come from the GameObject's Transform.
 */
struct LightComponent {
    /** Light type. */
    LightType type = LightType::Point;

    /** Light color (RGB, linear space). */
    float color[3] = {1.f, 1.f, 1.f};

    /** Intensity multiplier. For physically-based: Point uses lumens, Directional uses lux. */
    float intensity = 1.f;

    /** Range/radius for point and spot lights. Objects beyond this distance receive no light. */
    float range = 10.f;

    /** Falloff exponent for attenuation. 2.0 = physically correct inverse square. */
    float falloffExponent = 2.f;

    /** Spotlight inner cone angle (radians). Full brightness inside this angle. */
    float innerConeAngle = 0.5f;

    /** Spotlight outer cone angle (radians). Light fades to zero at this angle. */
    float outerConeAngle = 0.7f;

    /** Active flag. Inactive lights don't contribute to scene lighting. */
    bool bActive = true;

    /** Cast shadows flag (future). */
    bool bCastShadows = false;

    /** Index of the owning GameObject in the scene. Used for transform lookup. */
    uint32_t gameObjectIndex = 0;
};

/**
 * GPU-side light data structure.
 * Matches shader layout. 64 bytes per light for alignment.
 */
struct alignas(16) GpuLightData {
    float position[4];    /** xyz = world position, w = range */
    float direction[4];   /** xyz = direction (normalized), w = type (as float) */
    float color[4];       /** rgb = color, a = intensity */
    float params[4];      /** x = innerCone, y = outerCone, z = falloff, w = active (1.0 or 0.0) */
};
static_assert(sizeof(GpuLightData) == 64, "GpuLightData must be 64 bytes");

/** Maximum lights supported in a single scene. */
constexpr uint32_t kMaxLights = 256;

/** Size of light buffer header (light count + padding). */
constexpr uint32_t kLightBufferHeaderSize = 16;

/** Total light buffer size. */
constexpr uint32_t kLightBufferSize = kLightBufferHeaderSize + kMaxLights * sizeof(GpuLightData);

/**
 * Fill GPU light data from a LightComponent and its world transform.
 * @param light The light component.
 * @param worldPos World position (from Transform).
 * @param worldDir World direction (from Transform, for spot/directional).
 * @param gpu Output GPU data structure.
 */
inline void LightFillGpuData(const LightComponent& light,
                              const float* worldPos,
                              const float* worldDir,
                              GpuLightData& gpu) {
    gpu.position[0] = worldPos[0];
    gpu.position[1] = worldPos[1];
    gpu.position[2] = worldPos[2];
    gpu.position[3] = light.range;

    gpu.direction[0] = worldDir[0];
    gpu.direction[1] = worldDir[1];
    gpu.direction[2] = worldDir[2];
    gpu.direction[3] = static_cast<float>(static_cast<uint32_t>(light.type));

    gpu.color[0] = light.color[0];
    gpu.color[1] = light.color[1];
    gpu.color[2] = light.color[2];
    gpu.color[3] = light.intensity;

    gpu.params[0] = light.innerConeAngle;
    gpu.params[1] = light.outerConeAngle;
    gpu.params[2] = light.falloffExponent;
    gpu.params[3] = light.bActive ? 1.f : 0.f;
}

