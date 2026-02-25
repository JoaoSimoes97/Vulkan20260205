#include "gpu_culler.h"
#include "vulkan/vulkan_utils.h"
#include <cstring>
#include <stdexcept>

GPUCuller::~GPUCuller() {
    Destroy();
}

bool GPUCuller::Create(VkDevice device,
                       VkPhysicalDevice physicalDevice,
                       VulkanShaderManager* pShaderManager,
                       uint32_t maxObjects,
                       uint32_t maxBatches) {
    VulkanUtils::LogTrace("GPUCuller::Create: maxObjects={}, maxBatches={}", maxObjects, maxBatches);

    if (device == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("GPUCuller::Create: invalid device");
        return false;
    }
    if (pShaderManager == nullptr || !pShaderManager->IsValid()) {
        VulkanUtils::LogErr("GPUCuller::Create: invalid shader manager");
        return false;
    }
    if (maxObjects == 0) {
        VulkanUtils::LogErr("GPUCuller::Create: maxObjects must be > 0");
        return false;
    }

    m_device = device;
    m_physicalDevice = physicalDevice;
    m_maxObjects = maxObjects;
    m_maxBatches = maxBatches;
    // Each batch must be able to hold ALL objects (worst case: all objects in one batch)
    m_maxObjectsPerBatch = maxObjects;

    // Create GPU buffers
    // 1. Frustum UBO (small, host visible, updated per frame)
    if (!m_frustumBuffer.Create(device, physicalDevice,
                                 sizeof(FrustumData),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 true)) {
        VulkanUtils::LogErr("GPUCuller::Create: failed to create frustum buffer");
        Destroy();
        return false;
    }

    // 2. Cull input SSBO (all object bounds, host visible for CPU upload)
    VkDeviceSize cullInputSize = static_cast<VkDeviceSize>(maxObjects) * sizeof(CullObjectData);
    if (!m_cullInputBuffer.Create(device, physicalDevice,
                                   cullInputSize,
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   true)) {
        VulkanUtils::LogErr("GPUCuller::Create: failed to create cull input buffer");
        Destroy();
        return false;
    }

    // 3. Visible indices SSBO (output, GPU writes, host visible for readback)
    // Per-batch layout: each batch gets maxObjectsPerBatch slots
    // Total size = maxBatches * maxObjectsPerBatch to prevent overflow
    VkDeviceSize visibleIndicesSize = static_cast<VkDeviceSize>(m_maxBatches) * static_cast<VkDeviceSize>(m_maxObjectsPerBatch) * sizeof(uint32_t);
    if (!m_visibleIndicesBuffer.Create(device, physicalDevice,
                                        visibleIndicesSize,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        true)) {
        VulkanUtils::LogErr("GPUCuller::Create: failed to create visible indices buffer");
        Destroy();
        return false;
    }

    // 4. Atomic counter SSBO (single uint32, GPU atomics, host visible for readback)
    if (!m_atomicCounterBuffer.Create(device, physicalDevice,
                                       sizeof(uint32_t),
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       true)) {
        VulkanUtils::LogErr("GPUCuller::Create: failed to create atomic counter buffer");
        Destroy();
        return false;
    }

    // 5. Indirect commands SSBO (one command per batch, non-indexed draw)
    VkDeviceSize indirectSize = static_cast<VkDeviceSize>(maxBatches) * sizeof(DrawIndirectCommand);
    if (!m_indirectBuffer.Create(device, physicalDevice,
                                  indirectSize,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  true)) {
        VulkanUtils::LogErr("GPUCuller::Create: failed to create indirect buffer");
        Destroy();
        return false;
    }

    // 6. Per-batch atomic counters SSBO (one uint32 per batch)
    VkDeviceSize batchCountersSize = static_cast<VkDeviceSize>(maxBatches) * sizeof(uint32_t);
    if (!m_batchCountersBuffer.Create(device, physicalDevice,
                                       batchCountersSize,
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       true)) {
        VulkanUtils::LogErr("GPUCuller::Create: failed to create batch counters buffer");
        Destroy();
        return false;
    }

    // Create descriptor set layout
    if (!CreateDescriptorSetLayout()) {
        VulkanUtils::LogErr("GPUCuller::Create: failed to create descriptor set layout");
        Destroy();
        return false;
    }

    // Create descriptor pool
    if (!CreateDescriptorPool()) {
        VulkanUtils::LogErr("GPUCuller::Create: failed to create descriptor pool");
        Destroy();
        return false;
    }

    // Create descriptor set
    if (!CreateDescriptorSet()) {
        VulkanUtils::LogErr("GPUCuller::Create: failed to create descriptor set");
        Destroy();
        return false;
    }

    // Create compute pipeline
    ComputePipelineLayoutDescriptor layoutDesc;
    layoutDesc.descriptorSetLayouts.push_back(m_descriptorSetLayout);
    // No push constants for now

    try {
        m_computePipeline.Create(device, pShaderManager, "shaders/gpu_cull.comp.spv", layoutDesc);
    } catch (const std::exception& e) {
        VulkanUtils::LogErr("GPUCuller::Create: failed to create compute pipeline: {}", e.what());
        Destroy();
        return false;
    }

    VulkanUtils::LogInfo("GPUCuller created: maxObjects={}, maxBatches={}", maxObjects, maxBatches);
    return true;
}

void GPUCuller::Destroy() {
    m_computePipeline.Destroy();

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    m_descriptorSet = VK_NULL_HANDLE;  // Freed with pool

    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }

    m_frustumBuffer.Destroy();
    m_cullInputBuffer.Destroy();
    m_visibleIndicesBuffer.Destroy();
    m_atomicCounterBuffer.Destroy();
    m_indirectBuffer.Destroy();
    m_batchCountersBuffer.Destroy();

    m_device = VK_NULL_HANDLE;
    m_physicalDevice = VK_NULL_HANDLE;
    m_maxObjects = 0;
    m_maxBatches = 1;
    m_currentObjectCount = 0;
}

bool GPUCuller::CreateDescriptorSetLayout() {
    // Bindings match gpu_cull.comp
    VkDescriptorSetLayoutBinding bindings[6] = {};

    // Binding 0: Frustum UBO
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    // Binding 1: Cull input SSBO (read-only)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    // Binding 2: Visible indices SSBO (write)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[2].pImmutableSamplers = nullptr;

    // Binding 3: Global atomic counter SSBO (read-write, for stats)
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[3].pImmutableSamplers = nullptr;

    // Binding 4: Indirect commands SSBO (write)
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[4].pImmutableSamplers = nullptr;

    // Binding 5: Per-batch atomic counters SSBO (read-write)
    bindings[5].binding = 5;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[5].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 6,
        .pBindings = bindings,
    };

    VkResult r = vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout);
    return r == VK_SUCCESS;
}

bool GPUCuller::CreateDescriptorPool() {
    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 5;  // 5 SSBOs (bindings 1-5)

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = 1,
        .poolSizeCount = 2,
        .pPoolSizes = poolSizes,
    };

    VkResult r = vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool);
    return r == VK_SUCCESS;
}

bool GPUCuller::CreateDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = m_descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_descriptorSetLayout,
    };

    VkResult r = vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet);
    if (r != VK_SUCCESS) {
        return false;
    }

    // Write descriptor set
    VkDescriptorBufferInfo frustumInfo = {
        .buffer = m_frustumBuffer.GetBuffer(),
        .offset = 0,
        .range = sizeof(FrustumData),
    };
    VkDescriptorBufferInfo cullInputInfo = {
        .buffer = m_cullInputBuffer.GetBuffer(),
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkDescriptorBufferInfo visibleIndicesInfo = {
        .buffer = m_visibleIndicesBuffer.GetBuffer(),
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkDescriptorBufferInfo atomicCounterInfo = {
        .buffer = m_atomicCounterBuffer.GetBuffer(),
        .offset = 0,
        .range = sizeof(uint32_t),
    };
    VkDescriptorBufferInfo indirectInfo = {
        .buffer = m_indirectBuffer.GetBuffer(),
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkDescriptorBufferInfo batchCountersInfo = {
        .buffer = m_batchCountersBuffer.GetBuffer(),
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    VkWriteDescriptorSet writes[6] = {};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &frustumInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &cullInputInfo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = m_descriptorSet;
    writes[2].dstBinding = 2;
    writes[2].dstArrayElement = 0;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[2].descriptorCount = 1;
    writes[2].pBufferInfo = &visibleIndicesInfo;

    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = m_descriptorSet;
    writes[3].dstBinding = 3;
    writes[3].dstArrayElement = 0;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[3].descriptorCount = 1;
    writes[3].pBufferInfo = &atomicCounterInfo;

    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = m_descriptorSet;
    writes[4].dstBinding = 4;
    writes[4].dstArrayElement = 0;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[4].descriptorCount = 1;
    writes[4].pBufferInfo = &indirectInfo;

    writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[5].dstSet = m_descriptorSet;
    writes[5].dstBinding = 5;
    writes[5].dstArrayElement = 0;
    writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[5].descriptorCount = 1;
    writes[5].pBufferInfo = &batchCountersInfo;

    vkUpdateDescriptorSets(m_device, 6, writes, 0, nullptr);
    return true;
}

void GPUCuller::UpdateFrustum(const float planes[6][4], uint32_t objectCount, uint32_t batchCount) {
    m_currentObjectCount = (objectCount <= m_maxObjects) ? objectCount : m_maxObjects;
    m_currentBatchCount = (batchCount <= m_maxBatches) ? batchCount : m_maxBatches;
    if (m_currentBatchCount == 0) m_currentBatchCount = 1;

    FrustumData* pFrustum = static_cast<FrustumData*>(m_frustumBuffer.GetMappedPtr());
    if (pFrustum) {
        std::memcpy(pFrustum->planes, planes, sizeof(pFrustum->planes));
        pFrustum->objectCount = m_currentObjectCount;
        pFrustum->batchCount = m_currentBatchCount;
        pFrustum->maxObjectsPerBatch = m_maxObjectsPerBatch;
        pFrustum->_pad0 = 0;
    }
}

void GPUCuller::UploadCullObjects(const CullObjectData* pObjects, uint32_t count) {
    if (count == 0 || pObjects == nullptr) {
        return;
    }
    uint32_t uploadCount = (count <= m_maxObjects) ? count : m_maxObjects;

    void* pDst = m_cullInputBuffer.GetMappedPtr();
    if (pDst) {
        std::memcpy(pDst, pObjects, uploadCount * sizeof(CullObjectData));
    }
}

void GPUCuller::ResetCounters(VkCommandBuffer cmdBuffer) {
    // Reset global atomic counter to 0
    uint32_t* pCounter = static_cast<uint32_t*>(m_atomicCounterBuffer.GetMappedPtr());
    if (pCounter) {
        *pCounter = 0;
    }

    // Reset per-batch atomic counters to 0
    uint32_t* pBatchCounters = static_cast<uint32_t*>(m_batchCountersBuffer.GetMappedPtr());
    if (pBatchCounters) {
        for (uint32_t i = 0; i < m_maxBatches; ++i) {
            pBatchCounters[i] = 0;
        }
    }

    // Reset indirect command instance counts to 0
    // (firstInstance will be set by SetBatchDrawInfo or by GPU)
    DrawIndirectCommand* pCommands = static_cast<DrawIndirectCommand*>(m_indirectBuffer.GetMappedPtr());
    if (pCommands) {
        for (uint32_t i = 0; i < m_maxBatches; ++i) {
            pCommands[i].instanceCount = 0;
            pCommands[i].firstInstance = i * m_maxObjectsPerBatch;  // Per-batch section offset
        }
    }

    // Host writes are visible due to HOST_COHERENT, but we need a memory barrier
    // to ensure the compute shader sees the reset values
    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
    };

    vkCmdPipelineBarrier(cmdBuffer,
                         VK_PIPELINE_STAGE_HOST_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         1, &barrier,
                         0, nullptr,
                         0, nullptr);
}

void GPUCuller::Dispatch(VkCommandBuffer cmdBuffer) {
    if (m_currentObjectCount == 0) {
        return;
    }

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline.Get());
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_computePipeline.GetLayout(),
                            0, 1, &m_descriptorSet,
                            0, nullptr);

    // Workgroup size is 256 (defined in gpu_cull.comp)
    constexpr uint32_t WORKGROUP_SIZE = 256;
    uint32_t groupCountX = (m_currentObjectCount + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;

    vkCmdDispatch(cmdBuffer, groupCountX, 1, 1);
}

void GPUCuller::BarrierAfterDispatch(VkCommandBuffer cmdBuffer) {
    // Memory barrier: compute shader writes → vertex/indirect reads
    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
    };

    vkCmdPipelineBarrier(cmdBuffer,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                         0,
                         1, &barrier,
                         0, nullptr,
                         0, nullptr);
}

uint32_t GPUCuller::ReadbackVisibleCount() {
    uint32_t* pCounter = static_cast<uint32_t*>(m_atomicCounterBuffer.GetMappedPtr());
    return pCounter ? *pCounter : 0;
}

void GPUCuller::SetBatchDrawInfo(uint32_t batchId, uint32_t vertexCount, uint32_t firstVertex) {
    if (batchId >= m_maxBatches) {
        return;
    }

    DrawIndirectCommand* pCommands = static_cast<DrawIndirectCommand*>(m_indirectBuffer.GetMappedPtr());
    if (pCommands) {
        pCommands[batchId].vertexCount = vertexCount;
        pCommands[batchId].instanceCount = 0;  // GPU will write this
        pCommands[batchId].firstVertex = firstVertex;
        pCommands[batchId].firstInstance = batchId * m_maxObjectsPerBatch;  // offset into visible indices
    }
}
