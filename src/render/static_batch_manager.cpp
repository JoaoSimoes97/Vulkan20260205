#include "static_batch_manager.h"
#include "../vulkan/vulkan_utils.h"
#include <cstring>

namespace render {

StaticBatchManager::~StaticBatchManager() {
    Destroy();
}

bool StaticBatchManager::Create(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t maxInstances) {
    if (device == VK_NULL_HANDLE || maxInstances == 0) {
        VulkanUtils::LogErr("StaticBatchManager::Create - invalid parameters");
        return false;
    }

    m_device = device;
    m_physicalDevice = physicalDevice;
    m_maxInstances = maxInstances;

    // Reserve CPU-side storage
    m_instances.reserve(maxInstances);

    // Create GPU buffers (DEVICE_LOCAL for best GPU read performance)
    // We'll use staging buffers for uploads
    
    // Instance buffer: GPUInstanceData * maxInstances
    VkDeviceSize instanceBufferSize = sizeof(GPUInstanceData) * maxInstances;
    if (!m_instanceBuffer.Create(
            device, physicalDevice,
            instanceBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            false)) {
        VulkanUtils::LogErr("StaticBatchManager::Create - failed to create instance buffer");
        return false;
    }

    // Cull data buffer: GPUCullData * maxInstances
    VkDeviceSize cullDataBufferSize = sizeof(GPUCullData) * maxInstances;
    if (!m_cullDataBuffer.Create(
            device, physicalDevice,
            cullDataBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            false)) {
        VulkanUtils::LogErr("StaticBatchManager::Create - failed to create cull data buffer");
        m_instanceBuffer.Destroy();
        return false;
    }

    VulkanUtils::LogInfo("StaticBatchManager: Created with capacity for {} instances", maxInstances);
    return true;
}

void StaticBatchManager::Destroy() {
    Clear();
    m_instanceBuffer.Destroy();
    m_cullDataBuffer.Destroy();
    m_device = VK_NULL_HANDLE;
    m_physicalDevice = VK_NULL_HANDLE;
    m_maxInstances = 0;
    m_uploaded = false;
}

uint32_t StaticBatchManager::AddInstance(InstanceTier tier,
                                          const glm::mat4& transform,
                                          uint32_t meshIndex,
                                          uint32_t materialIndex,
                                          const glm::vec4& boundingSphere) {
    if (tier != InstanceTier::Static && tier != InstanceTier::SemiStatic) {
        VulkanUtils::LogErr("StaticBatchManager::AddInstance - invalid tier (must be Static or SemiStatic)");
        return UINT32_MAX;
    }

    if (m_instances.size() >= m_maxInstances) {
        VulkanUtils::LogErr("StaticBatchManager::AddInstance - capacity exceeded");
        return UINT32_MAX;
    }

    uint32_t instanceId = static_cast<uint32_t>(m_instances.size());

    InstanceEntry entry{};
    entry.tier = tier;
    entry.instanceData.model = transform;
    entry.cullData.boundingSphere = boundingSphere;
    entry.cullData.meshIndex = meshIndex;
    entry.cullData.materialIndex = materialIndex;
    entry.cullData.instanceIndex = instanceId;
    entry.cullData._pad = 0;

    m_instances.push_back(entry);

    // Track in batch map
    BatchKey key{ meshIndex, materialIndex };
    m_batches[key].push_back(instanceId);

    // If already uploaded, mark as dirty for partial update
    if (m_uploaded) {
        m_dirtySet.insert(instanceId);
    }

    return instanceId;
}

void StaticBatchManager::UpdateTransform(uint32_t instanceId, const glm::mat4& newTransform) {
    if (instanceId >= m_instances.size()) {
        VulkanUtils::LogErr("StaticBatchManager::UpdateTransform - invalid instanceId");
        return;
    }

    auto& entry = m_instances[instanceId];
    if (entry.tier != InstanceTier::SemiStatic) {
        VulkanUtils::LogWarn("StaticBatchManager::UpdateTransform - updating Static tier instance (consider SemiStatic)");
    }

    entry.instanceData.model = newTransform;
    m_dirtySet.insert(instanceId);
}

void StaticBatchManager::MarkDirty(uint32_t instanceId) {
    if (instanceId < m_instances.size()) {
        m_dirtySet.insert(instanceId);
    }
}

bool StaticBatchManager::UploadToGPU() {
    if (m_instances.empty()) {
        VulkanUtils::LogWarn("StaticBatchManager::UploadToGPU - no instances to upload");
        m_uploaded = true;
        return true;
    }

    // Create staging buffers for upload
    VkDeviceSize instanceDataSize = sizeof(GPUInstanceData) * m_instances.size();
    VkDeviceSize cullDataSize = sizeof(GPUCullData) * m_instances.size();

    GPUBuffer stagingInstance;
    GPUBuffer stagingCull;

    if (!stagingInstance.Create(
            m_device, m_physicalDevice,
            instanceDataSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true)) {
        VulkanUtils::LogErr("StaticBatchManager::UploadToGPU - failed to create staging instance buffer");
        return false;
    }

    if (!stagingCull.Create(
            m_device, m_physicalDevice,
            cullDataSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true)) {
        VulkanUtils::LogErr("StaticBatchManager::UploadToGPU - failed to create staging cull buffer");
        stagingInstance.Destroy();
        return false;
    }

    // Copy data to staging buffers
    auto* pInstanceData = static_cast<GPUInstanceData*>(stagingInstance.GetMappedPtr());
    auto* pCullData = static_cast<GPUCullData*>(stagingCull.GetMappedPtr());

    for (size_t i = 0; i < m_instances.size(); ++i) {
        pInstanceData[i] = m_instances[i].instanceData;
        pCullData[i] = m_instances[i].cullData;
    }

    // TODO: Record command buffer to copy staging -> device local
    // For now, we'll need to pass a queue/command pool or use a deferred upload mechanism
    // This is a placeholder - actual implementation needs command buffer recording

    VulkanUtils::LogInfo("StaticBatchManager::UploadToGPU - {} instances staged (needs copy cmd)", m_instances.size());

    stagingInstance.Destroy();
    stagingCull.Destroy();

    m_uploaded = true;
    m_dirtySet.clear();
    return true;
}

uint32_t StaticBatchManager::FlushDirty() {
    if (m_dirtySet.empty() || !m_uploaded) {
        return 0;
    }

    // For partial updates, we could use a staging buffer and copy only dirty regions
    // For simplicity, marking this as a future optimization
    uint32_t count = static_cast<uint32_t>(m_dirtySet.size());

    VulkanUtils::LogTrace("StaticBatchManager::FlushDirty - {} instances marked dirty (partial update pending)", count);

    // TODO: Implement partial GPU buffer update
    // Options:
    // 1. Use HOST_VISIBLE buffer directly (simpler but less optimal)
    // 2. Use staging + sparse copy commands
    // 3. Use VK_EXT_host_query_reset for direct writes

    m_dirtySet.clear();
    return count;
}

void StaticBatchManager::Clear() {
    m_instances.clear();
    m_dirtySet.clear();
    m_batches.clear();
    m_uploaded = false;
}

} // namespace render
