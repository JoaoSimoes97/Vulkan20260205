#pragma once

#include "vulkan/vulkan_command_buffers.h"
#include <map>
#include <string>
#include <vector>

class Scene;
class MaterialManager;
class MeshManager;
class PipelineManager;
class VulkanShaderManager;

/**
 * Builds draw list from scene: resolve material -> pipeline/layout, mesh -> draw params.
 * Sorts by (pipeline, mesh) to reduce state changes. Reuse one vector per frame (clear + fill).
 * Descriptor sets per pipeline: pass map pipelineKey -> sets so any pipeline can bind sets without hardcoding.
 */
class RenderListBuilder {
public:
    RenderListBuilder() = default;

    /**
     * Build draw calls from current scene. Fills outDrawCalls (cleared first).
     * viewProj: optional column-major 4x4 for frustum culling (object position in clip space); null = no culling.
     * Objects must have pushData already filled (e.g. viewProj * transform, color).
     * Push constant size is validated against material layout; oversized pushes are skipped.
     * pPipelineDescriptorSets_ic: optional. For each pipeline key, the descriptor sets to bind (set 0, 1, ...).
     */
    void Build(std::vector<DrawCall>& vecOutDrawCalls_out,
               const Scene* pScene_ic,
               VkDevice pDevice_ic,
               VkRenderPass pRenderPass_ic,
               bool bRenderPassHasDepth_ic,
               PipelineManager* pPipelineManager_ic,
               MaterialManager* pMaterialManager_ic,
               VulkanShaderManager* pShaderManager_ic,
               const float* pViewProj_ic = nullptr,
               const std::map<std::string, std::vector<VkDescriptorSet>>* pPipelineDescriptorSets_ic = nullptr);
};
