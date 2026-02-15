#pragma once

#include "vulkan_shader_manager.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <vector>

struct PipelineLayoutDescriptor {
    std::vector<VkPushConstantRange> pushConstantRanges;
};

inline bool operator==(const PipelineLayoutDescriptor& a, const PipelineLayoutDescriptor& b) {
    if (a.pushConstantRanges.size() != b.pushConstantRanges.size())
        return false;
    for (size_t i = 0; i < a.pushConstantRanges.size(); ++i) {
        const auto& ra = a.pushConstantRanges[i];
        const auto& rb = b.pushConstantRanges[i];
        if (ra.stageFlags != rb.stageFlags || ra.offset != rb.offset || ra.size != rb.size)
            return false;
    }
    return true;
}

struct GraphicsPipelineParams {
    VkPrimitiveTopology    topology;
    VkBool32               primitiveRestartEnable;
    VkPolygonMode          polygonMode;
    VkCullModeFlags        cullMode;
    VkFrontFace            frontFace;
    float                  lineWidth;
    VkSampleCountFlagBits  rasterizationSamples;
    VkBool32               depthTestEnable  = VK_TRUE;
    VkBool32               depthWriteEnable  = VK_TRUE;
    VkCompareOp            depthCompareOp    = VK_COMPARE_OP_LESS_OR_EQUAL;
    VkBool32               blendEnable       = VK_FALSE;
    VkBlendFactor          srcColorBlendFactor  = VK_BLEND_FACTOR_SRC_ALPHA;
    VkBlendFactor          dstColorBlendFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    VkBlendOp              colorBlendOp     = VK_BLEND_OP_ADD;
    VkBlendFactor          srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor          dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    VkBlendOp              alphaBlendOp     = VK_BLEND_OP_ADD;
};

inline bool operator==(const GraphicsPipelineParams& a, const GraphicsPipelineParams& b) {
    return a.topology == b.topology && a.primitiveRestartEnable == b.primitiveRestartEnable
        && a.polygonMode == b.polygonMode && a.cullMode == b.cullMode && a.frontFace == b.frontFace
        && a.lineWidth == b.lineWidth && a.rasterizationSamples == b.rasterizationSamples
        && a.depthTestEnable == b.depthTestEnable && a.depthWriteEnable == b.depthWriteEnable
        && a.depthCompareOp == b.depthCompareOp
        && a.blendEnable == b.blendEnable && a.srcColorBlendFactor == b.srcColorBlendFactor
        && a.dstColorBlendFactor == b.dstColorBlendFactor && a.colorBlendOp == b.colorBlendOp
        && a.srcAlphaBlendFactor == b.srcAlphaBlendFactor && a.dstAlphaBlendFactor == b.dstAlphaBlendFactor
        && a.alphaBlendOp == b.alphaBlendOp;
}

/*
 * Graphics pipeline: vert + frag stages, fixed-function state. Holds shared_ptr to shader modules
 * so shaders stay alive while the pipeline exists; when pipeline is destroyed the shared_ptrs are dropped.
 */
class VulkanPipeline {
public:
    VulkanPipeline() = default;
    ~VulkanPipeline();

    void Create(VkDevice pDevice_ic, VkRenderPass renderPass_ic,
                VulkanShaderManager* pShaderManager_ic,
                const std::string& sVertPath_ic, const std::string& sFragPath_ic,
                const GraphicsPipelineParams& stPipelineParams_ic,
                const PipelineLayoutDescriptor& stLayoutDescriptor_ic,
                bool bRenderPassHasDepth_ic);
    void Destroy();

    VkPipeline Get() const { return this->m_pipeline; }
    VkPipelineLayout GetLayout() const { return this->m_pipelineLayout; }
    bool IsValid() const { return this->m_pipeline != VK_NULL_HANDLE; }

private:
    VkDevice         m_device = VK_NULL_HANDLE;
    VkPipeline       m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    ShaderModulePtr  m_pVertShader;
    ShaderModulePtr  m_pFragShader;
};
