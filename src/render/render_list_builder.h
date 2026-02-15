#pragma once

#include "vulkan/vulkan_command_buffers.h"
#include <vector>

class Scene;
class MaterialManager;
class MeshManager;
class PipelineManager;
class VulkanShaderManager;

/**
 * Builds draw list from scene: resolve material -> pipeline/layout, mesh -> draw params.
 * Sorts by (pipeline, mesh) to reduce state changes. Reuse one vector per frame (clear + fill).
 */
class RenderListBuilder {
public:
    RenderListBuilder() = default;

    /**
     * Build draw calls from current scene. Fills outDrawCalls (cleared first).
     * Objects must have pushData already filled (e.g. viewProj * transform, color).
     * Sort order: pipeline, then vertexCount/firstVertex (mesh proxy).
     */
    void Build(std::vector<DrawCall>& outDrawCalls,
               const Scene* pScene,
               VkDevice device,
               VkRenderPass renderPass,
               bool renderPassHasDepth,
               PipelineManager* pPipelineManager,
               MaterialManager* pMaterialManager,
               VulkanShaderManager* pShaderManager);
};
