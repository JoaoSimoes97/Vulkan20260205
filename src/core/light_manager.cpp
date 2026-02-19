/*
 * LightManager — Implementation.
 */
#include "light_manager.h"
#include "scene_new.h"
#include <cstring>
#include <stdexcept>

LightManager::~LightManager() {
    Destroy();
}

void LightManager::Create(VkDevice device, VkPhysicalDevice physicalDevice) {
    m_device = device;
    m_physicalDevice = physicalDevice;

    // Create light buffer (host-visible for easy updates)
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = kLightBufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_lightBuffer) != VK_SUCCESS)
        throw std::runtime_error("LightManager: Failed to create light buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, m_lightBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_lightBufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(m_device, m_lightBuffer, nullptr);
        m_lightBuffer = VK_NULL_HANDLE;
        throw std::runtime_error("LightManager: Failed to allocate light buffer memory");
    }

    vkBindBufferMemory(m_device, m_lightBuffer, m_lightBufferMemory, 0);

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
    const auto& transforms = m_pScene->GetTransforms();
    const auto& lights = m_pScene->GetLights();

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

        // Get world position and direction from transform
        const Transform& t = transforms[go.transformIndex];

        float worldDir[3];
        TransformGetForward(t, worldDir[0], worldDir[1], worldDir[2]);

        LightFillGpuData(light, t.position, worldDir, pLights[lightCount]);
        ++lightCount;
    }

    // Write header (light count)
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

uint32_t LightManager::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("LightManager: Failed to find suitable memory type");
}

