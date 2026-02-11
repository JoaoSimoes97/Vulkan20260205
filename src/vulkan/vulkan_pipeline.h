#pragma once

#include "vulkan_shader_manager.h"
#include <vulkan/vulkan.h>
#include <string>

/*
 * Parameters for graphics pipeline fixed-function state. Caller must set every member;
 * pass to VulkanPipeline::Create (no default).
 */
struct GraphicsPipelineParams {
    /** Primitive type: triangle list, line list, point list, or strip variants. */
    VkPrimitiveTopology    topology;
    /** Enable primitive restart for strip topologies (e.g. 0xFFFFFFFF in index buffer). */
    VkBool32               primitiveRestartEnable;
    /** Fill, wireframe (LINE), or point rasterization. */
    VkPolygonMode          polygonMode;
    /** Which faces to cull: NONE, BACK, FRONT, or FRONT_AND_BACK. */
    VkCullModeFlags        cullMode;
    /** Vertex winding for front face: COUNTER_CLOCKWISE or CLOCKWISE. */
    VkFrontFace            frontFace;
    /** Line width for line/point modes; wide lines require wideLines device feature. */
    float                  lineWidth;
    /** MSAA sample count; must match render pass and framebuffer. */
    VkSampleCountFlagBits  rasterizationSamples;
};

inline bool operator==(const GraphicsPipelineParams& a, const GraphicsPipelineParams& b) {
    return a.topology == b.topology && a.primitiveRestartEnable == b.primitiveRestartEnable
        && a.polygonMode == b.polygonMode && a.cullMode == b.cullMode && a.frontFace == b.frontFace
        && a.lineWidth == b.lineWidth && a.rasterizationSamples == b.rasterizationSamples;
}

/*
 * Graphics pipeline: vert + frag stages, fixed-function state (viewport, raster, blend, etc.).
 * Depends on render pass and swapchain extent. Loads shaders via VulkanShaderManager and holds
 * refs until Destroy. Use GraphicsPipelineParams to configure topology, rasterization, and MSAA.
 */
class VulkanPipeline {
public:
    VulkanPipeline() = default;
    ~VulkanPipeline();

    void Create(VkDevice device, VkExtent2D extent, VkRenderPass renderPass,
                VulkanShaderManager* pShaderManager,
                const std::string& sVertPath, const std::string& sFragPath,
                const GraphicsPipelineParams& pipelineParams);
    void Destroy();

    VkPipeline Get() const { return this->m_pipeline; }
    VkPipelineLayout GetLayout() const { return this->m_pipelineLayout; }
    bool IsValid() const { return this->m_pipeline != VK_NULL_HANDLE; }

private:
    VkDevice         m_device = VK_NULL_HANDLE;
    VkPipeline       m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VulkanShaderManager* m_pShaderManager = nullptr;
    std::string      m_sVertPath;
    std::string      m_sFragPath;
};
