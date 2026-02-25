#pragma once

#include "vulkan_shader_manager.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

/**
 * Descriptor set layout info for compute pipelines.
 */
struct ComputePipelineLayoutDescriptor {
    std::vector<VkPushConstantRange>   pushConstantRanges;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
};

/**
 * Simple compute pipeline wrapper.
 * Unlike graphics pipelines, compute pipelines only need:
 * - One shader stage (compute)
 * - Pipeline layout (descriptor sets + push constants)
 * No render pass, no vertex input, no rasterization state.
 */
class VulkanComputePipeline {
public:
    VulkanComputePipeline() = default;
    ~VulkanComputePipeline();

    /**
     * Create compute pipeline from a .comp shader.
     * @param pDevice_ic Vulkan device
     * @param pShaderManager_ic Shader manager for loading SPIR-V
     * @param sCompPath_ic Path to compiled .comp.spv shader
     * @param stLayoutDescriptor_ic Push constants and descriptor set layouts
     */
    void Create(VkDevice pDevice_ic,
                VulkanShaderManager* pShaderManager_ic,
                const std::string& sCompPath_ic,
                const ComputePipelineLayoutDescriptor& stLayoutDescriptor_ic);
    
    void Destroy();

    VkPipeline Get() const { return m_pipeline; }
    VkPipelineLayout GetLayout() const { return m_pipelineLayout; }
    bool IsValid() const { return m_pipeline != VK_NULL_HANDLE; }

private:
    VkDevice         m_device = VK_NULL_HANDLE;
    VkPipeline       m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    ShaderModulePtr  m_pCompShader;
};
