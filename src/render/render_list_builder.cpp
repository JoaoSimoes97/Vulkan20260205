/*
 * RenderListBuilder — build draw list from scene; sort by (pipeline, mesh) to reduce binds.
 */
#include "render_list_builder.h"
#include "managers/material_manager.h"
#include "managers/mesh_manager.h"
#include "managers/pipeline_manager.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "vulkan/vulkan_shader_manager.h"
#include <algorithm>

namespace {
    bool DrawCallOrder(const DrawCall& a, const DrawCall& b) {
        if (a.pipeline != b.pipeline)
            return a.pipeline < b.pipeline;
        if (a.vertexBuffer != b.vertexBuffer)
            return a.vertexBuffer < b.vertexBuffer;
        if (a.vertexCount != b.vertexCount)
            return a.vertexCount < b.vertexCount;
        return a.firstVertex < b.firstVertex;
    }
}

void RenderListBuilder::Build(std::vector<DrawCall>& outDrawCalls,
                              const Scene* pScene,
                              VkDevice device,
                              VkRenderPass renderPass,
                              bool renderPassHasDepth,
                              PipelineManager* pPipelineManager,
                              MaterialManager* pMaterialManager,
                              VulkanShaderManager* pShaderManager) {
    outDrawCalls.clear();
    if (pScene == nullptr || pPipelineManager == nullptr || pMaterialManager == nullptr || pShaderManager == nullptr)
        return;
    if (device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE)
        return;

    const std::vector<Object>& objects = pScene->GetObjects();
    outDrawCalls.reserve(objects.size());

    for (const auto& obj : objects) {
        if (!obj.pMaterial || !obj.pMesh || !obj.pMesh->HasValidBuffer() || obj.pushDataSize == 0u || obj.pushData.empty())
            continue;
        VkPipeline pipe = obj.pMaterial->GetPipelineIfReady(device, renderPass, pPipelineManager, pShaderManager, renderPassHasDepth);
        VkPipelineLayout layout = obj.pMaterial->GetPipelineLayoutIfReady(pPipelineManager);
        if (pipe == VK_NULL_HANDLE || layout == VK_NULL_HANDLE)
            continue;
        const uint32_t vc = obj.pMesh->GetVertexCount();
        if (vc == 0u)
            continue;
        outDrawCalls.push_back({
            .pipeline          = pipe,
            .pipelineLayout    = layout,
            .vertexBuffer      = obj.pMesh->GetVertexBuffer(),
            .vertexBufferOffset = obj.pMesh->GetVertexBufferOffset(),
            .pPushConstants    = obj.pushData.data(),
            .pushConstantSize  = obj.pushDataSize,
            .vertexCount       = vc,
            .instanceCount     = obj.pMesh->GetInstanceCount(),
            .firstVertex       = obj.pMesh->GetFirstVertex(),
            .firstInstance     = obj.pMesh->GetFirstInstance()
        });
    }

    std::sort(outDrawCalls.begin(), outDrawCalls.end(), DrawCallOrder);
}
