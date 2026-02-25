#include "render_system.h"
#include "../vulkan/vulkan_utils.h"

namespace render {

RenderSystem::~RenderSystem() {
    Destroy();
}

bool RenderSystem::Create(VkDevice device,
                          VkPhysicalDevice physicalDevice,
                          const RenderSystemConfig& config) {
    if (device == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("RenderSystem::Create - invalid device");
        return false;
    }

    m_device = device;
    m_physicalDevice = physicalDevice;
    m_config = config;

    // Initialize static batch manager (Tier 0/1)
    if (!m_staticBatchManager.Create(device, physicalDevice, config.maxStaticInstances)) {
        VulkanUtils::LogErr("RenderSystem::Create - failed to create StaticBatchManager");
        return false;
    }

    // Initialize GPU culler
    if (!m_gpuCuller.Create(device, physicalDevice, 
                            config.maxStaticInstances, 
                            config.maxMeshes,
                            "shaders/gpu_cull.spv")) {
        VulkanUtils::LogErr("RenderSystem::Create - failed to create GPUCuller");
        m_staticBatchManager.Destroy();
        return false;
    }

    // Initialize dynamic instance ring buffer (Tier 2)
    if (!m_dynamicInstanceBuffer.Create(device, physicalDevice,
                                        config.maxDynamicInstances,
                                        config.framesInFlight,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) {
        VulkanUtils::LogErr("RenderSystem::Create - failed to create dynamic instance buffer");
        m_gpuCuller.Destroy();
        m_staticBatchManager.Destroy();
        return false;
    }

    m_dynamicInstances.reserve(config.maxDynamicInstances);

    VulkanUtils::LogInfo("RenderSystem: Created with {} static, {} dynamic, {} mesh slots",
                         config.maxStaticInstances, config.maxDynamicInstances, config.maxMeshes);

    m_initialized = true;
    return true;
}

void RenderSystem::Destroy() {
    if (!m_initialized) return;

    m_dynamicInstanceBuffer.Destroy();
    m_gpuCuller.Destroy();
    m_staticBatchManager.Destroy();

    m_dynamicInstances.clear();
    m_device = VK_NULL_HANDLE;
    m_initialized = false;
    m_staticFinalized = false;
}

// ---- Instance Registration ----

uint32_t RenderSystem::RegisterStaticInstance(InstanceTier tier,
                                              const glm::mat4& transform,
                                              uint32_t meshIndex,
                                              uint32_t materialIndex,
                                              const glm::vec4& boundingSphere) {
    if (m_staticFinalized) {
        VulkanUtils::LogWarn("RenderSystem: Cannot add static instance after finalization");
        return UINT32_MAX;
    }

    return m_staticBatchManager.AddInstance(tier, transform, meshIndex, materialIndex, boundingSphere);
}

void RenderSystem::UpdateStaticTransform(uint32_t instanceId, const glm::mat4& newTransform) {
    m_staticBatchManager.UpdateTransform(instanceId, newTransform);
}

bool RenderSystem::FinalizeStaticInstances() {
    if (m_staticFinalized) {
        VulkanUtils::LogWarn("RenderSystem: Static instances already finalized");
        return true;
    }

    if (!m_staticBatchManager.UploadToGPU()) {
        VulkanUtils::LogErr("RenderSystem: Failed to upload static instances to GPU");
        return false;
    }

    m_staticFinalized = true;
    VulkanUtils::LogInfo("RenderSystem: Finalized {} static instances", 
                         m_staticBatchManager.GetInstanceCount());
    return true;
}

// ---- Frame Flow ----

void RenderSystem::BeginFrame(uint32_t frameIndex) {
    m_currentFrame = frameIndex;
    m_dynamicInstances.clear();

    // Flush any dirty static instances
    if (m_staticFinalized && m_staticBatchManager.HasDirty()) {
        m_staticBatchManager.FlushDirty();
    }
}

void RenderSystem::UpdateCamera(const glm::mat4& viewProj) {
    m_gpuCuller.UpdateFrustum(viewProj);
}

uint32_t RenderSystem::AddDynamicInstance(const glm::mat4& transform,
                                          uint32_t meshIndex,
                                          uint32_t materialIndex) {
    if (m_dynamicInstances.size() >= m_config.maxDynamicInstances) {
        VulkanUtils::LogWarn("RenderSystem: Dynamic instance capacity exceeded");
        return UINT32_MAX;
    }

    uint32_t index = static_cast<uint32_t>(m_dynamicInstances.size());
    
    DynamicInstance inst{};
    inst.instanceData.model = transform;
    inst.meshIndex = meshIndex;
    inst.materialIndex = materialIndex;

    m_dynamicInstances.push_back(inst);
    return index;
}

void RenderSystem::DispatchGPUCulling(VkCommandBuffer cmd) {
    if (!m_staticFinalized || m_staticBatchManager.GetInstanceCount() == 0) {
        return;
    }

    // Reset counters
    m_gpuCuller.ResetCounters(cmd);

    // Dispatch culling
    m_gpuCuller.Dispatch(cmd,
                         m_staticBatchManager.GetInstanceBuffer(),
                         m_staticBatchManager.GetCullDataBuffer(),
                         m_staticBatchManager.GetInstanceCount());
}

void RenderSystem::InsertPostCullBarrier(VkCommandBuffer cmd) {
    m_gpuCuller.InsertBarrier(cmd);
}

void RenderSystem::DrawStaticInstances(VkCommandBuffer cmd) {
    if (!m_staticFinalized || m_staticBatchManager.GetInstanceCount() == 0) {
        return;
    }

    // TODO: Implement indirect drawing using GPUCuller output buffers
    // This requires:
    // 1. Binding the visible indices buffer as instance data source
    // 2. Calling vkCmdDrawIndexedIndirectCount for each mesh batch
    //
    // For now, this is a placeholder - full implementation requires
    // integration with the main rendering pipeline

    VulkanUtils::LogTrace("RenderSystem::DrawStaticInstances - {} instances ready for indirect draw",
                          m_staticBatchManager.GetInstanceCount());
}

uint32_t RenderSystem::DrawDynamicInstances(VkCommandBuffer cmd) {
    if (m_dynamicInstances.empty()) {
        return 0;
    }

    // Upload dynamic instances to ring buffer
    void* mapped = m_dynamicInstanceBuffer.GetMappedFrame(m_currentFrame);
    if (mapped) {
        GPUInstanceData* instanceData = static_cast<GPUInstanceData*>(mapped);
        for (size_t i = 0; i < m_dynamicInstances.size(); ++i) {
            instanceData[i] = m_dynamicInstances[i].instanceData;
        }
    }

    // TODO: Issue draw calls for dynamic instances
    // This requires integration with mesh/material system
    //
    // For now, this is a placeholder that returns the count

    return static_cast<uint32_t>(m_dynamicInstances.size());
}

void RenderSystem::EndFrame() {
    // Nothing to do currently - reserved for future cleanup
}

void RenderSystem::Clear() {
    m_staticBatchManager.Clear();
    m_dynamicInstances.clear();
    m_staticFinalized = false;
}

} // namespace render
