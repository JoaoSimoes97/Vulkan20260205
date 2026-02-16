/*
 * RenderListBuilder — build draw list from scene; sort by (pipeline, mesh); optional frustum culling and push size validation.
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
    bool DrawCallOrder(const DrawCall& stA_ic, const DrawCall& stB_ic) {
        if (stA_ic.pipeline != stB_ic.pipeline)
            return stA_ic.pipeline < stB_ic.pipeline;
        if (stA_ic.vertexBuffer != stB_ic.vertexBuffer)
            return stA_ic.vertexBuffer < stB_ic.vertexBuffer;
        if (stA_ic.vertexCount != stB_ic.vertexCount)
            return stA_ic.vertexCount < stB_ic.vertexCount;
        return stA_ic.firstVertex < stB_ic.firstVertex;
    }

    /** Object position from column-major localTransform (translation part). */
    void ObjectPosition(const float* pLocalTransform_ic, float& fX_out, float& fY_out, float& fZ_out) {
        fX_out = pLocalTransform_ic[12];
        fY_out = pLocalTransform_ic[13];
        fZ_out = pLocalTransform_ic[14];
    }

    /** Transform point (x,y,z,1) by column-major viewProj; write clip-space (cx, cy, cz, cw). */
    void TransformToClip(const float* pViewProj_ic, float fX_ic, float fY_ic, float fZ_ic, float& fCx_out, float& fCy_out, float& fCz_out, float& fCw_out) {
        fCx_out = pViewProj_ic[0]*fX_ic + pViewProj_ic[4]*fY_ic + pViewProj_ic[8]*fZ_ic + pViewProj_ic[12];
        fCy_out = pViewProj_ic[1]*fX_ic + pViewProj_ic[5]*fY_ic + pViewProj_ic[9]*fZ_ic + pViewProj_ic[13];
        fCz_out = pViewProj_ic[2]*fX_ic + pViewProj_ic[6]*fY_ic + pViewProj_ic[10]*fZ_ic + pViewProj_ic[14];
        fCw_out = pViewProj_ic[3]*fX_ic + pViewProj_ic[7]*fY_ic + pViewProj_ic[11]*fZ_ic + pViewProj_ic[15];
    }

    /** True if clip-space point (cx,cy,cz,cw) is inside Vulkan NDC (-1..1, -1..1, 0..1). */
    bool InsideFrustum(float fCx_ic, float fCy_ic, float fCz_ic, float fCw_ic) {
        if (fCw_ic <= static_cast<float>(0.f))
            return false;
        return (fCx_ic >= -fCw_ic) && (fCx_ic <= fCw_ic) && (fCy_ic >= -fCw_ic) && (fCy_ic <= fCw_ic) && (fCz_ic >= static_cast<float>(0.f)) && (fCz_ic <= fCw_ic);
    }

    /** Max byte size allowed by layout's push constant ranges. */
    uint32_t MaxPushConstantSize(const PipelineLayoutDescriptor& stLayout_ic) {
        uint32_t lMaxEnd = static_cast<uint32_t>(0);
        for (const auto& stR : stLayout_ic.pushConstantRanges)
            lMaxEnd = std::max(lMaxEnd, static_cast<uint32_t>(stR.offset + stR.size));
        return lMaxEnd;
    }
}

void RenderListBuilder::Build(std::vector<DrawCall>& vecOutDrawCalls_out,
                              const Scene* pScene_ic,
                              VkDevice pDevice_ic,
                              VkRenderPass pRenderPass_ic,
                              bool bRenderPassHasDepth_ic,
                              PipelineManager* pPipelineManager_ic,
                              MaterialManager* pMaterialManager_ic,
                              VulkanShaderManager* pShaderManager_ic,
                              const float* pViewProj_ic,
                              const std::map<std::string, std::vector<VkDescriptorSet>>* pPipelineDescriptorSets_ic) {
    vecOutDrawCalls_out.clear();
    if ((pScene_ic == nullptr) || (pPipelineManager_ic == nullptr) || (pMaterialManager_ic == nullptr) || (pShaderManager_ic == nullptr))
        return;
    if ((pDevice_ic == VK_NULL_HANDLE) || (pRenderPass_ic == VK_NULL_HANDLE))
        return;

    const std::vector<Object>& vecObjects = pScene_ic->GetObjects();
    vecOutDrawCalls_out.reserve(vecObjects.size());

    for (const auto& obj : vecObjects) {
        if ((obj.pMaterial == nullptr) || (obj.pMesh == nullptr) || (obj.pMesh->HasValidBuffer() == false) || (obj.pushDataSize == 0u) || (obj.pushData.empty() == true))
            continue;
        const uint32_t lMaxPush = MaxPushConstantSize(obj.pMaterial->layoutDescriptor);
        if ((lMaxPush > 0u) && (obj.pushDataSize > lMaxPush))
            continue;
        if (pViewProj_ic != nullptr) {
            float fPx = static_cast<float>(0.f);
            float fPy = static_cast<float>(0.f);
            float fPz = static_cast<float>(0.f);
            ObjectPosition(obj.localTransform, fPx, fPy, fPz);
            float fCx = static_cast<float>(0.f);
            float fCy = static_cast<float>(0.f);
            float fCz = static_cast<float>(0.f);
            float fCw = static_cast<float>(0.f);
            TransformToClip(pViewProj_ic, fPx, fPy, fPz, fCx, fCy, fCz, fCw);
            if (InsideFrustum(fCx, fCy, fCz, fCw) == false)
                continue;
        }
        VkPipeline pPipe = obj.pMaterial->GetPipelineIfReady(pDevice_ic, pRenderPass_ic, pPipelineManager_ic, pShaderManager_ic, bRenderPassHasDepth_ic);
        VkPipelineLayout pLayout = obj.pMaterial->GetPipelineLayoutIfReady(pPipelineManager_ic);
        if ((pPipe == VK_NULL_HANDLE) || (pLayout == VK_NULL_HANDLE))
            continue;
        const uint32_t lVc = obj.pMesh->GetVertexCount();
        if (lVc == 0u)
            continue;
        DrawCall stD = {
            .pipeline           = pPipe,
            .pipelineLayout     = pLayout,
            .vertexBuffer       = obj.pMesh->GetVertexBuffer(),
            .vertexBufferOffset = obj.pMesh->GetVertexBufferOffset(),
            .pPushConstants      = obj.pushData.data(),
            .pushConstantSize   = obj.pushDataSize,
            .vertexCount        = lVc,
            .instanceCount      = obj.pMesh->GetInstanceCount(),
            .firstVertex        = obj.pMesh->GetFirstVertex(),
            .firstInstance      = obj.pMesh->GetFirstInstance(),
            .descriptorSets     = {},
            .instanceBuffer     = VK_NULL_HANDLE,
            .instanceBufferOffset = 0,
        };
        if (pPipelineDescriptorSets_ic != nullptr) {
            auto it = pPipelineDescriptorSets_ic->find(obj.pMaterial->pipelineKey);
            if (it != pPipelineDescriptorSets_ic->end() && !it->second.empty())
                stD.descriptorSets = it->second;
        }
        /* Skip draws that require descriptor sets but have none (e.g. main/wire before default texture is ready). */
        if (!obj.pMaterial->layoutDescriptor.descriptorSetLayouts.empty() && stD.descriptorSets.empty())
            continue;
        vecOutDrawCalls_out.push_back(stD);
    }

    std::sort(vecOutDrawCalls_out.begin(), vecOutDrawCalls_out.end(), DrawCallOrder);
}
