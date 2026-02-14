#pragma once

#include "vulkan/vulkan_pipeline.h"
#include "vulkan/vulkan_shader_manager.h"
#include <map>
#include <string>
#include <vulkan/vulkan.h>

/*
 * Pipeline manager: request pipelines by key (vert+frag paths); loads are non-blocking and scalable.
 * GetPipelineIfReady(key, ..., layoutDescriptor) returns VkPipeline when shaders are ready; pipeline
 * layout is taken from layoutDescriptor per key (different keys can have different push constant layouts).
 * Pipeline is recreated when renderPass, params, or layout descriptor change.
 * Call DestroyPipelines() when swapchain/render pass change (e.g. resize); next GetPipelineIfReady recreates when shaders land.
 */
class PipelineManager {
public:
    PipelineManager() = default;

    /* Request a pipeline by key; submits shader loads without blocking. Idempotent per key. */
    void RequestPipeline(const std::string& sKey,
                         VulkanShaderManager* pShaderManager,
                         const std::string& sVertPath,
                         const std::string& sFragPath);

    /*
     * Non-blocking: return VkPipeline for key if shaders are ready and pipeline is built for this renderPass/params/layout.
     * layoutDescriptor defines push constant ranges (and later descriptor set layouts) for this pipeline.
     * Returns VK_NULL_HANDLE if not ready or load failed.
     */
    VkPipeline GetPipelineIfReady(const std::string& sKey,
                                  VkDevice device,
                                  VkRenderPass renderPass,
                                  VulkanShaderManager* pShaderManager,
                                  const GraphicsPipelineParams& pipelineParams,
                                  const PipelineLayoutDescriptor& layoutDescriptor);

    VkPipelineLayout GetPipelineLayoutIfReady(const std::string& sKey) const;

    /* Destroy all cached pipelines (e.g. on swapchain recreate). Shader requests remain; next GetPipelineIfReady recreates. */
    void DestroyPipelines();

private:
    struct PipelineEntry {
        std::string                sVertPath;
        std::string                sFragPath;
        VulkanPipeline             pipeline;
        VkRenderPass               renderPass = VK_NULL_HANDLE;
        GraphicsPipelineParams     lastParams = {};
        PipelineLayoutDescriptor   lastLayout = {};
    };
    std::map<std::string, PipelineEntry> m_entries;
};
