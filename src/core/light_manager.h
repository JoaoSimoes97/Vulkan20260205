/*
 * LightManager — Manages all lights in a scene, handles GPU upload.
 */
#pragma once

#include "light_component.h"
#include <vector>
#include <vulkan/vulkan.h>

class SceneNew;

/**
 * Emissive light data for injection from emissive objects.
 */
struct EmissiveLightData {
    float position[3];   // World position of emissive object
    float radius;        // Estimated light radius based on object size
    float color[3];      // Emissive color (linear RGB)
    float intensity;     // Emissive strength
};

/**
 * LightManager — Tracks scene lights, culls, and uploads to GPU.
 */
class LightManager {
public:
    LightManager() = default;
    ~LightManager();

    /** Initialize Vulkan resources (light buffer). */
    void Create(VkDevice device, VkPhysicalDevice physicalDevice);

    /** Destroy Vulkan resources. */
    void Destroy();

    /** Set the scene to read lights from. */
    void SetScene(SceneNew* pScene) { m_pScene = pScene; }

    /** Update GPU light buffer from scene lights. Call each frame before rendering. */
    void UpdateLightBuffer();

    /** Inject additional lights from emissive objects (append to scene lights). */
    void InjectEmissiveLights(const std::vector<EmissiveLightData>& emissiveLights);

    /** Get the light buffer for descriptor set binding. */
    VkBuffer GetLightBuffer() const { return m_lightBuffer; }

    /** Get the size of valid data in the light buffer. */
    VkDeviceSize GetLightBufferSize() const { return kLightBufferSize; }

    /** Get descriptor buffer info for binding. */
    VkDescriptorBufferInfo GetDescriptorBufferInfo() const;

    /** Get number of active lights in the scene. */
    uint32_t GetActiveLightCount() const { return m_activeLightCount; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    SceneNew* m_pScene = nullptr;

    VkBuffer m_lightBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_lightBufferMemory = VK_NULL_HANDLE;
    void* m_mappedMemory = nullptr;

    uint32_t m_activeLightCount = 0;

    /** Find suitable memory type for buffer allocation. */
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

