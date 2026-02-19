#pragma once

#include "vulkan/vulkan_pipeline.h"
#include "vulkan/vulkan_shader_manager.h"
#include <map>
#include <memory>
#include <string>
#include <shared_mutex>
#include <vulkan/vulkan.h>

class PipelineManager;
struct PipelineHandle;

/*
 * Material: describes how to draw (pipeline key + layout + rendering state). Resolves to
 * shared_ptr<PipelineHandle> via PipelineManager and caches it so materials keep pipelines alive.
 * Used by scene objects; when no object uses a material, TrimUnused() drops it.
 */
struct MaterialHandle {
    std::string                 pipelineKey;
    PipelineLayoutDescriptor    layoutDescriptor;
    GraphicsPipelineParams      pipelineParams;

    MaterialHandle() = default;
    MaterialHandle(std::string key, PipelineLayoutDescriptor layout, GraphicsPipelineParams params);

    /** Resolve to pipeline for current device/render pass; caches shared_ptr<PipelineHandle>; returns VK_NULL_HANDLE if not ready. */
    VkPipeline GetPipelineIfReady(VkDevice device,
                                  VkRenderPass renderPass,
                                  PipelineManager* pPipelineManager,
                                  VulkanShaderManager* pShaderManager,
                                  bool renderPassHasDepth);

    VkPipelineLayout GetPipelineLayoutIfReady(PipelineManager* pPipelineManager);

private:
    mutable std::shared_ptr<PipelineHandle> m_pCachedPipeline;
};

/*
 * Registry: material id -> shared_ptr<MaterialHandle>. RegisterMaterial(id, ...) creates and
 * returns a handle; GetMaterial(id) returns cached handle. TrimUnused() removes entries where
 * no object holds a ref (use_count() == 1).
 */
class MaterialManager {
public:
    MaterialManager() = default;

    /* Register a material; returns shared_ptr so caller can hold it. Idempotent per id: returns existing if already registered. */
    std::shared_ptr<MaterialHandle> RegisterMaterial(const std::string& sMaterialId,
                                                      const std::string& sPipelineKey,
                                                      const PipelineLayoutDescriptor& layoutDescriptor,
                                                      const GraphicsPipelineParams& pipelineParams);

    std::shared_ptr<MaterialHandle> GetMaterial(const std::string& sMaterialId) const;

    /* Remove cache entries where use_count() == 1. Call e.g. once per frame. */
    void TrimUnused();

private:
    mutable std::shared_mutex m_mutex;
    std::map<std::string, std::shared_ptr<MaterialHandle>> m_registry;
};
