/*
 * LightManager — Implementation.
 */
#include "light_manager.h"
#include "scene/scene_unified.h"
#include "vulkan/vulkan_utils.h"
#include <cstring>
#include <cstdio>
#include <stdexcept>

LightManager::~LightManager() {
    Destroy();
}

void LightManager::Create(VkDevice device, VkPhysicalDevice physicalDevice) {
    m_device = device;
    m_physicalDevice = physicalDevice;

    // Create light buffer (host-visible for easy updates)
    if (VulkanUtils::CreateBuffer(m_device, m_physicalDevice, kLightBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &m_lightBuffer, &m_lightBufferMemory) != VK_SUCCESS)
        throw std::runtime_error("LightManager: Failed to create light buffer");

    // Map permanently for updates
    vkMapMemory(m_device, m_lightBufferMemory, 0, kLightBufferSize, 0, &m_mappedMemory);

    // Initialize with zero lights
    std::memset(m_mappedMemory, 0, kLightBufferSize);
}

void LightManager::Destroy() {
    if (m_device == VK_NULL_HANDLE) return;

    if (m_mappedMemory != nullptr) {
        vkUnmapMemory(m_device, m_lightBufferMemory);
        m_mappedMemory = nullptr;
    }

    if (m_lightBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_lightBuffer, nullptr);
        m_lightBuffer = VK_NULL_HANDLE;
    }

    if (m_lightBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_lightBufferMemory, nullptr);
        m_lightBufferMemory = VK_NULL_HANDLE;
    }

    m_device = VK_NULL_HANDLE;
}

void LightManager::UpdateLightBuffer() {
    if (m_pScene == nullptr || m_mappedMemory == nullptr)
        return;

    uint8_t* pData = static_cast<uint8_t*>(m_mappedMemory);

    // Count active lights and write to buffer
    const auto& gameObjects = m_pScene->GetGameObjects();
    const auto& lights = m_pScene->GetLights();

    // Debug: log counts once at startup
    static bool bLoggedOnce = false;
    if (!bLoggedOnce) {
        printf("[LightManager] gameObjects=%zu, lights=%zu\n", gameObjects.size(), lights.size());
        bLoggedOnce = true;
    }

    uint32_t lightCount = 0;
    GpuLightData* pLights = reinterpret_cast<GpuLightData*>(pData + kLightBufferHeaderSize);

    for (const auto& go : gameObjects) {
        if (!go.bActive || !go.HasLight())
            continue;

        const LightComponent& light = lights[go.lightIndex];
        if (!light.bActive)
            continue;

        if (lightCount >= kMaxLights)
            break;

        const Transform* pT = m_pScene->GetTransform(go.id);
        if (!pT) continue;
        const Transform& t = *pT;

        float worldDir[3];
        TransformGetForward(t, worldDir[0], worldDir[1], worldDir[2]);
        float worldPos[3] = { t.worldMatrix[12], t.worldMatrix[13], t.worldMatrix[14] };

        LightFillGpuData(light, worldPos, worldDir, pLights[lightCount]);
        ++lightCount;
    }

    // Write header (light count)
    std::memcpy(pData, &lightCount, sizeof(uint32_t));
    m_activeLightCount = lightCount;
    
    // Debug: log light count once
    static bool bLoggedLightCount = false;
    if (!bLoggedLightCount && lightCount > 0) {
        const GpuLightData& l0 = pLights[0];
        printf("[LightManager] lightCount=%u, light0: dir=(%.3f, %.3f, %.3f), color=(%.2f, %.2f, %.2f), intensity=%.2f, type=%.0f, active=%.0f\n",
               lightCount, l0.direction[0], l0.direction[1], l0.direction[2],
               l0.color[0], l0.color[1], l0.color[2], l0.color[3],
               l0.direction[3], l0.params[3]);
        bLoggedLightCount = true;
    }
}

void LightManager::InjectEmissiveLights(const std::vector<EmissiveLightData>& emissiveLights) {
    if (m_mappedMemory == nullptr || emissiveLights.empty())
        return;

    uint8_t* pData = static_cast<uint8_t*>(m_mappedMemory);
    GpuLightData* pLights = reinterpret_cast<GpuLightData*>(pData + kLightBufferHeaderSize);
    
    uint32_t lightCount = m_activeLightCount;
    
    for (const EmissiveLightData& emissive : emissiveLights) {
        if (lightCount >= kMaxLights)
            break;

        // Create point light from emissive data
        GpuLightData& gpuLight = pLights[lightCount];
        gpuLight.position[0] = emissive.position[0];
        gpuLight.position[1] = emissive.position[1];
        gpuLight.position[2] = emissive.position[2];
        gpuLight.position[3] = emissive.radius; // Range for attenuation cutoff
        
        gpuLight.direction[0] = 0.0f;
        gpuLight.direction[1] = -1.0f;
        gpuLight.direction[2] = 0.0f;
        gpuLight.direction[3] = 1.0f; // Type = point light
        
        gpuLight.color[0] = emissive.color[0];
        gpuLight.color[1] = emissive.color[1];
        gpuLight.color[2] = emissive.color[2];
        gpuLight.color[3] = emissive.intensity;
        
        gpuLight.params[0] = 0.0f; // innerCone (unused for point)
        gpuLight.params[1] = 0.0f; // outerCone (unused for point)
        gpuLight.params[2] = 2.0f; // falloff (standard inverse-square)
        gpuLight.params[3] = 1.0f; // active
        
        ++lightCount;
    }
    
    // Update header with new count
    std::memcpy(pData, &lightCount, sizeof(uint32_t));
    m_activeLightCount = lightCount;
}

VkDescriptorBufferInfo LightManager::GetDescriptorBufferInfo() const {
    VkDescriptorBufferInfo info = {};
    info.buffer = m_lightBuffer;
    info.offset = 0;
    info.range = kLightBufferSize;
    return info;
}

