#include "gpu_culler.h"
#include "../vulkan/vulkan_utils.h"
#include <fstream>
#include <cstring>
#include <cmath>

namespace render {

/* ---- Load SPIR-V shader file ---- */
static std::vector<char> LoadShaderFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return {};
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
}

/* ---- Create shader module ---- */
static VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& code) {
    if (code.empty()) return VK_NULL_HANDLE;
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod = VK_NULL_HANDLE;
    vkCreateShaderModule(device, &ci, nullptr, &mod);
    return mod;
}

GPUCuller::~GPUCuller() {
    Destroy();
}

bool GPUCuller::Create(VkDevice device,
                       VkPhysicalDevice physicalDevice,
                       uint32_t maxInstances,
                       uint32_t maxMeshes,
                       const char* cullShaderPath) {
    if (device == VK_NULL_HANDLE || maxInstances == 0 || maxMeshes == 0) {
        VulkanUtils::LogErr("GPUCuller::Create - invalid parameters");
        return false;
    }

    m_device = device;
    m_physicalDevice = physicalDevice;
    m_maxInstances = maxInstances;
    m_maxMeshes = maxMeshes;

    if (!CreateDescriptorSetLayout()) return false;
    if (!CreatePipelineLayout()) return false;
    if (!CreatePipeline(cullShaderPath)) return false;
    if (!CreateOutputBuffers()) return false;
    if (!CreateDescriptorPool()) return false;
    if (!CreateDescriptorSet()) return false;

    VulkanUtils::LogInfo("GPUCuller: Created with maxInstances={}, maxMeshes={}", maxInstances, maxMeshes);
    return true;
}

void GPUCuller::Destroy() {
    if (m_device == VK_NULL_HANDLE) return;

    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

    m_cullUniformBuffer.Destroy();
    m_visibleIndicesBuffer.Destroy();
    m_indirectCommandsBuffer.Destroy();

    m_pipeline = VK_NULL_HANDLE;
    m_pipelineLayout = VK_NULL_HANDLE;
    m_descriptorSetLayout = VK_NULL_HANDLE;
    m_descriptorPool = VK_NULL_HANDLE;
    m_descriptorSet = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}

bool GPUCuller::CreateDescriptorSetLayout() {
    // Bindings:
    // 0: CullUniforms (uniform buffer)
    // 1: Instance transforms (storage buffer, read-only)
    // 2: Cull data (storage buffer, read-only)
    // 3: Visible indices output (storage buffer)
    // 4: Indirect commands output (storage buffer)
    
    VkDescriptorSetLayoutBinding bindings[5] = {};
    
    // Binding 0: Cull uniforms
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 1: Instance data (read-only)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 2: Cull data (read-only)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 3: Visible indices output
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 4: Indirect commands output
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 5;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        VulkanUtils::LogErr("GPUCuller: Failed to create descriptor set layout");
        return false;
    }
    return true;
}

bool GPUCuller::CreatePipelineLayout() {
    // Push constant for instance count
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(CullPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_descriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        VulkanUtils::LogErr("GPUCuller: Failed to create pipeline layout");
        return false;
    }
    return true;
}

bool GPUCuller::CreatePipeline(const char* shaderPath) {
    std::string fullPath = VulkanUtils::GetResourcePath(shaderPath);
    auto shaderCode = LoadShaderFile(fullPath);
    if (shaderCode.empty()) {
        VulkanUtils::LogErr("GPUCuller: Failed to load shader from {}", fullPath);
        return false;
    }

    VkShaderModule shaderModule = CreateShaderModule(m_device, shaderCode);
    if (shaderModule == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("GPUCuller: Failed to create shader module");
        return false;
    }

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shaderModule;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = m_pipelineLayout;

    VkResult result = vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
    vkDestroyShaderModule(m_device, shaderModule, nullptr);

    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("GPUCuller: Failed to create compute pipeline");
        return false;
    }
    return true;
}

bool GPUCuller::CreateOutputBuffers() {
    // Uniform buffer for frustum planes (host visible for easy updates)
    if (!m_cullUniformBuffer.Create(
            m_device, m_physicalDevice,
            sizeof(CullUniforms),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true)) {
        VulkanUtils::LogErr("GPUCuller: Failed to create uniform buffer");
        return false;
    }

    // Visible indices buffer: count (uint32) + indices (uint32 * maxInstances)
    VkDeviceSize visibleBufferSize = sizeof(uint32_t) + sizeof(uint32_t) * m_maxInstances;
    if (!m_visibleIndicesBuffer.Create(
            m_device, m_physicalDevice,
            visibleBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            false)) {
        VulkanUtils::LogErr("GPUCuller: Failed to create visible indices buffer");
        return false;
    }

    // Indirect commands buffer: DrawIndexedIndirectCommand * maxMeshes
    VkDeviceSize indirectBufferSize = sizeof(GPUDrawIndirectCommand) * m_maxMeshes;
    if (!m_indirectCommandsBuffer.Create(
            m_device, m_physicalDevice,
            indirectBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            false)) {
        VulkanUtils::LogErr("GPUCuller: Failed to create indirect commands buffer");
        return false;
    }

    return true;
}

bool GPUCuller::CreateDescriptorPool() {
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 }  // instance, cull, visible, indirect
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        VulkanUtils::LogErr("GPUCuller: Failed to create descriptor pool");
        return false;
    }
    return true;
}

bool GPUCuller::CreateDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;

    if (vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        VulkanUtils::LogErr("GPUCuller: Failed to allocate descriptor set");
        return false;
    }

    // Update descriptor set with uniform buffer and output buffers
    // Note: Instance and cull data buffers are bound at dispatch time
    
    VkDescriptorBufferInfo uniformBufferInfo{};
    uniformBufferInfo.buffer = m_cullUniformBuffer.GetBuffer();
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range = sizeof(CullUniforms);

    VkDescriptorBufferInfo visibleBufferInfo{};
    visibleBufferInfo.buffer = m_visibleIndicesBuffer.GetBuffer();
    visibleBufferInfo.offset = 0;
    visibleBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo indirectBufferInfo{};
    indirectBufferInfo.buffer = m_indirectCommandsBuffer.GetBuffer();
    indirectBufferInfo.offset = 0;
    indirectBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet writes[3] = {};
    
    // Binding 0: Uniform buffer
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &uniformBufferInfo;

    // Binding 3: Visible indices output
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_descriptorSet;
    writes[1].dstBinding = 3;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &visibleBufferInfo;

    // Binding 4: Indirect commands output
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = m_descriptorSet;
    writes[2].dstBinding = 4;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[2].pBufferInfo = &indirectBufferInfo;

    vkUpdateDescriptorSets(m_device, 3, writes, 0, nullptr);
    return true;
}

void GPUCuller::UpdateFrustum(const glm::mat4& viewProj) {
    ExtractFrustumPlanes(viewProj);
    m_cullUniforms.viewProj = viewProj;

    // Upload to GPU
    void* mapped = m_cullUniformBuffer.GetMappedPtr();
    if (mapped) {
        memcpy(mapped, &m_cullUniforms, sizeof(CullUniforms));
    }
}

void GPUCuller::ExtractFrustumPlanes(const glm::mat4& vp) {
    // Extract frustum planes from view-projection matrix
    // Each plane equation: ax + by + cz + d = 0 where (a,b,c) is normal, d is distance
    // Plane format: vec4(normal.xyz, distance)
    
    // Left: row3 + row0
    m_cullUniforms.frustumPlanes[0] = glm::vec4(
        vp[0][3] + vp[0][0],
        vp[1][3] + vp[1][0],
        vp[2][3] + vp[2][0],
        vp[3][3] + vp[3][0]
    );
    
    // Right: row3 - row0
    m_cullUniforms.frustumPlanes[1] = glm::vec4(
        vp[0][3] - vp[0][0],
        vp[1][3] - vp[1][0],
        vp[2][3] - vp[2][0],
        vp[3][3] - vp[3][0]
    );
    
    // Bottom: row3 + row1
    m_cullUniforms.frustumPlanes[2] = glm::vec4(
        vp[0][3] + vp[0][1],
        vp[1][3] + vp[1][1],
        vp[2][3] + vp[2][1],
        vp[3][3] + vp[3][1]
    );
    
    // Top: row3 - row1
    m_cullUniforms.frustumPlanes[3] = glm::vec4(
        vp[0][3] - vp[0][1],
        vp[1][3] - vp[1][1],
        vp[2][3] - vp[2][1],
        vp[3][3] - vp[3][1]
    );
    
    // Near: row3 + row2
    m_cullUniforms.frustumPlanes[4] = glm::vec4(
        vp[0][3] + vp[0][2],
        vp[1][3] + vp[1][2],
        vp[2][3] + vp[2][2],
        vp[3][3] + vp[3][2]
    );
    
    // Far: row3 - row2
    m_cullUniforms.frustumPlanes[5] = glm::vec4(
        vp[0][3] - vp[0][2],
        vp[1][3] - vp[1][2],
        vp[2][3] - vp[2][2],
        vp[3][3] - vp[3][2]
    );

    // Normalize planes
    for (int i = 0; i < 6; i++) {
        float len = glm::length(glm::vec3(m_cullUniforms.frustumPlanes[i]));
        if (len > 0.0001f) {
            m_cullUniforms.frustumPlanes[i] /= len;
        }
    }
}

void GPUCuller::ResetCounters(VkCommandBuffer cmd) {
    // Reset visible count to 0
    vkCmdFillBuffer(cmd, m_visibleIndicesBuffer.GetBuffer(), 0, sizeof(uint32_t), 0);
    
    // Reset all indirect command instance counts to 0
    // Note: We need to preserve indexCount, firstIndex, vertexOffset values
    // For simplicity, we zero the instanceCount field only
    // This is done by zeroing the whole buffer and re-initializing mesh info
    vkCmdFillBuffer(cmd, m_indirectCommandsBuffer.GetBuffer(), 0, 
                    m_maxMeshes * sizeof(GPUDrawIndirectCommand), 0);

    // Memory barrier to ensure fill completes before compute
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        1, &barrier,
        0, nullptr,
        0, nullptr);
}

void GPUCuller::Dispatch(VkCommandBuffer cmd,
                         VkBuffer instanceBuffer,
                         VkBuffer cullDataBuffer,
                         uint32_t instanceCount) {
    if (instanceCount == 0) return;

    // Update descriptor set with input buffers
    VkDescriptorBufferInfo instanceBufferInfo{};
    instanceBufferInfo.buffer = instanceBuffer;
    instanceBufferInfo.offset = 0;
    instanceBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo cullDataBufferInfo{};
    cullDataBufferInfo.buffer = cullDataBuffer;
    cullDataBufferInfo.offset = 0;
    cullDataBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet writes[2] = {};
    
    // Binding 1: Instance data
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_descriptorSet;
    writes[0].dstBinding = 1;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &instanceBufferInfo;

    // Binding 2: Cull data
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_descriptorSet;
    writes[1].dstBinding = 2;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &cullDataBufferInfo;

    vkUpdateDescriptorSets(m_device, 2, writes, 0, nullptr);

    // Bind pipeline and descriptor set
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 
                            0, 1, &m_descriptorSet, 0, nullptr);

    // Push constants
    CullPushConstants pc{};
    pc.instanceCount = instanceCount;
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 
                       0, sizeof(CullPushConstants), &pc);

    // Dispatch: 256 threads per workgroup
    uint32_t workgroupCount = (instanceCount + 255) / 256;
    vkCmdDispatch(cmd, workgroupCount, 1, 1);
}

void GPUCuller::InsertBarrier(VkCommandBuffer cmd) {
    // Memory barrier: compute write -> indirect draw read
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        1, &barrier,
        0, nullptr,
        0, nullptr);
}

} // namespace render
