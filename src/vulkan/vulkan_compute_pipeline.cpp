#include "vulkan_compute_pipeline.h"
#include "vulkan_utils.h"
#include <stdexcept>

void VulkanComputePipeline::Create(VkDevice pDevice_ic,
                                   VulkanShaderManager* pShaderManager_ic,
                                   const std::string& sCompPath_ic,
                                   const ComputePipelineLayoutDescriptor& stLayoutDescriptor_ic) {
    VulkanUtils::LogTrace("VulkanComputePipeline::Create: {}", sCompPath_ic);
    
    if (pDevice_ic == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("VulkanComputePipeline::Create: invalid device");
        throw std::runtime_error("VulkanComputePipeline::Create: invalid device");
    }
    if (pShaderManager_ic == nullptr || !pShaderManager_ic->IsValid()) {
        VulkanUtils::LogErr("VulkanComputePipeline::Create: invalid shader manager");
        throw std::runtime_error("VulkanComputePipeline::Create: invalid shader manager");
    }

    m_device = pDevice_ic;
    
    // Load compute shader
    m_pCompShader = pShaderManager_ic->GetShader(pDevice_ic, sCompPath_ic);
    if (m_pCompShader == nullptr) {
        VulkanUtils::LogErr("VulkanComputePipeline::Create: failed to load compute shader: {}", sCompPath_ic);
        throw std::runtime_error("VulkanComputePipeline::Create: failed to load compute shader");
    }

    VkShaderModule compModule = *m_pCompShader.get();

    // Compute shader stage
    VkPipelineShaderStageCreateInfo stCompStage = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext  = nullptr,
        .flags  = 0,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = compModule,
        .pName  = "main",
        .pSpecializationInfo = nullptr,
    };

    // Pipeline layout
    VkPipelineLayoutCreateInfo stLayoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = static_cast<uint32_t>(stLayoutDescriptor_ic.descriptorSetLayouts.size()),
        .pSetLayouts            = stLayoutDescriptor_ic.descriptorSetLayouts.empty() 
                                    ? nullptr 
                                    : stLayoutDescriptor_ic.descriptorSetLayouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(stLayoutDescriptor_ic.pushConstantRanges.size()),
        .pPushConstantRanges    = stLayoutDescriptor_ic.pushConstantRanges.empty()
                                    ? nullptr
                                    : stLayoutDescriptor_ic.pushConstantRanges.data(),
    };

    VkResult r = vkCreatePipelineLayout(pDevice_ic, &stLayoutInfo, nullptr, &m_pipelineLayout);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("VulkanComputePipeline::Create: vkCreatePipelineLayout failed: {}", static_cast<int>(r));
        m_pCompShader.reset();
        throw std::runtime_error("VulkanComputePipeline::Create: failed to create pipeline layout");
    }

    // Compute pipeline
    VkComputePipelineCreateInfo stPipelineInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext              = nullptr,
        .flags              = 0,
        .stage              = stCompStage,
        .layout             = m_pipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex  = -1,
    };

    r = vkCreateComputePipelines(pDevice_ic, VK_NULL_HANDLE, 1, &stPipelineInfo, nullptr, &m_pipeline);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("VulkanComputePipeline::Create: vkCreateComputePipelines failed: {}", static_cast<int>(r));
        vkDestroyPipelineLayout(pDevice_ic, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
        m_pCompShader.reset();
        throw std::runtime_error("VulkanComputePipeline::Create: failed to create compute pipeline");
    }

    VulkanUtils::LogInfo("VulkanComputePipeline created: {}", sCompPath_ic);
}

void VulkanComputePipeline::Destroy() {
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    m_pCompShader.reset();
    m_device = VK_NULL_HANDLE;
}

VulkanComputePipeline::~VulkanComputePipeline() {
    Destroy();
}
