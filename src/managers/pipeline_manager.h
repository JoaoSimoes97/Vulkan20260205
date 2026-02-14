#pragma once

#include "vulkan/vulkan_pipeline.h"
#include "vulkan/vulkan_shader_manager.h"
#include <map>
#include <string>
#include <vulkan/vulkan.h>

/*
 * Pipeline manager: request pipelines by key (vert+frag paths); loads are non-blocking and scalable.
 * GetPipelineIfReady(key) returns VkPipeline when shaders are ready; if not ready, re-requests loads for that key and returns null (no wait).
 * Multiple pipelines: each key has its own vert/frag; manager re-requests per key when needed after DestroyPipelines().
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
     * Non-blocking: return VkPipeline for key if shaders are ready and pipeline is built for this renderPass/params.
     * Pipeline is recreated when renderPass or params change (viewport is dynamic, so extent is not needed).
     * Returns VK_NULL_HANDLE if not ready or load failed.
     */
    VkPipeline GetPipelineIfReady(const std::string& sKey,
                                  VkDevice device,
                                  VkRenderPass renderPass,
                                  VulkanShaderManager* pShaderManager,
                                  const GraphicsPipelineParams& pipelineParams);

    VkPipelineLayout GetPipelineLayoutIfReady(const std::string& sKey) const;

    /* Destroy all cached pipelines (e.g. on swapchain recreate). Shader requests remain; next GetPipelineIfReady recreates. */
    void DestroyPipelines();

private:
    struct PipelineEntry {
        std::string              sVertPath;
        std::string              sFragPath;
        VulkanPipeline           pipeline;
        VkRenderPass             renderPass = VK_NULL_HANDLE;
        GraphicsPipelineParams   lastParams = {};
    };
    std::map<std::string, PipelineEntry> m_entries;
};
