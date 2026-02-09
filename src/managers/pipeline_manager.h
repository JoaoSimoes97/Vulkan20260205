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
     * Non-blocking: return VkPipeline for key if shaders are ready and pipeline is built for this extent/renderPass.
     * Returns VK_NULL_HANDLE if not ready or load failed. When extent/renderPass change, pipeline is recreated.
     */
    VkPipeline GetPipelineIfReady(const std::string& sKey,
                                  VkDevice device,
                                  VkExtent2D extent,
                                  VkRenderPass renderPass,
                                  VulkanShaderManager* pShaderManager);

    VkPipelineLayout GetPipelineLayoutIfReady(const std::string& sKey) const;

    /* Destroy all cached pipelines (e.g. on swapchain recreate). Shader requests remain; next GetPipelineIfReady recreates. */
    void DestroyPipelines();

private:
    struct PipelineEntry {
        std::string    sVertPath;
        std::string    sFragPath;
        VulkanPipeline pipeline;
        VkExtent2D     extent   = { 0u, 0u };
        VkRenderPass  renderPass = VK_NULL_HANDLE;
    };
    std::map<std::string, PipelineEntry> m_entries;
};
