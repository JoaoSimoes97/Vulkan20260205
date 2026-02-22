/*
 * RenderListBuilder — build draw list from scene; sort by (pipeline, mesh); proper frustum culling with bounding spheres.
 */
#include "render_list_builder.h"
#include "managers/material_manager.h"
#include "managers/mesh_manager.h"
#include "managers/pipeline_manager.h"
#include "managers/texture_manager.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "vulkan/vulkan_shader_manager.h"
#include <algorithm>
#include <cmath>

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

    bool IsTransparentPipelineKey(const std::string& pipelineKey) {
        return pipelineKey.find("transparent") != std::string::npos;
    }

    /**
     * Frustum planes extracted from view-projection matrix (Gribb/Hartmann method).
     * Each plane is (A, B, C, D) where Ax + By + Cz + D = 0.
     * Normalized so (A,B,C) is unit length for distance calculations.
     */
    struct FrustumPlanes {
        float planes[6][4]; // Left, Right, Bottom, Top, Near, Far
        
        void ExtractFromViewProj(const float* vp) {
            // Row-major extraction from column-major matrix
            // Left: row3 + row0
            planes[0][0] = vp[3] + vp[0];
            planes[0][1] = vp[7] + vp[4];
            planes[0][2] = vp[11] + vp[8];
            planes[0][3] = vp[15] + vp[12];
            
            // Right: row3 - row0
            planes[1][0] = vp[3] - vp[0];
            planes[1][1] = vp[7] - vp[4];
            planes[1][2] = vp[11] - vp[8];
            planes[1][3] = vp[15] - vp[12];
            
            // Bottom: row3 + row1
            planes[2][0] = vp[3] + vp[1];
            planes[2][1] = vp[7] + vp[5];
            planes[2][2] = vp[11] + vp[9];
            planes[2][3] = vp[15] + vp[13];
            
            // Top: row3 - row1
            planes[3][0] = vp[3] - vp[1];
            planes[3][1] = vp[7] - vp[5];
            planes[3][2] = vp[11] - vp[9];
            planes[3][3] = vp[15] - vp[13];
            
            // Near: row3 + row2 (Vulkan: 0 to 1 depth)
            planes[4][0] = vp[3] + vp[2];
            planes[4][1] = vp[7] + vp[6];
            planes[4][2] = vp[11] + vp[10];
            planes[4][3] = vp[15] + vp[14];
            
            // Far: row3 - row2
            planes[5][0] = vp[3] - vp[2];
            planes[5][1] = vp[7] - vp[6];
            planes[5][2] = vp[11] - vp[10];
            planes[5][3] = vp[15] - vp[14];
            
            // Normalize all planes
            for (int i = 0; i < 6; ++i) {
                float len = std::sqrt(planes[i][0]*planes[i][0] + planes[i][1]*planes[i][1] + planes[i][2]*planes[i][2]);
                if (len > 0.0001f) {
                    float invLen = 1.0f / len;
                    planes[i][0] *= invLen;
                    planes[i][1] *= invLen;
                    planes[i][2] *= invLen;
                    planes[i][3] *= invLen;
                }
            }
        }
        
        /**
         * Test if sphere is visible (not completely outside any plane).
         * Returns true if sphere intersects or is inside frustum.
         */
        bool IsSphereVisible(float cx, float cy, float cz, float radius) const {
            for (int i = 0; i < 6; ++i) {
                // Distance from sphere center to plane
                float dist = planes[i][0]*cx + planes[i][1]*cy + planes[i][2]*cz + planes[i][3];
                // If sphere is completely on negative side, it's outside
                if (dist < -radius) {
                    return false;
                }
            }
            return true;
        }
    };

    /**
     * Compute world-space bounding sphere from mesh AABB and object transform.
     */
    void ComputeWorldBoundingSphere(const Object& obj, float& cx, float& cy, float& cz, float& radius) {
        if (obj.pMesh == nullptr) {
            // Fallback: use object position with small radius
            cx = obj.localTransform[12];
            cy = obj.localTransform[13];
            cz = obj.localTransform[14];
            radius = 1.0f;
            return;
        }
        
        const MeshAABB& aabb = obj.pMesh->GetAABB();
        if (!aabb.IsValid()) {
            // Fallback: use object position with default radius
            cx = obj.localTransform[12];
            cy = obj.localTransform[13];
            cz = obj.localTransform[14];
            radius = 2.0f;
            return;
        }
        
        // Get local AABB center
        float localCx, localCy, localCz;
        aabb.GetCenter(localCx, localCy, localCz);
        
        // Transform center to world space
        const float* m = obj.localTransform;
        cx = m[0]*localCx + m[4]*localCy + m[8]*localCz + m[12];
        cy = m[1]*localCx + m[5]*localCy + m[9]*localCz + m[13];
        cz = m[2]*localCx + m[6]*localCy + m[10]*localCz + m[14];
        
        // Compute world-space radius (account for non-uniform scale)
        float localRadius = aabb.GetBoundingSphereRadius();
        
        // Get max scale from transform columns
        float scaleX = std::sqrt(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
        float scaleY = std::sqrt(m[4]*m[4] + m[5]*m[5] + m[6]*m[6]);
        float scaleZ = std::sqrt(m[8]*m[8] + m[9]*m[9] + m[10]*m[10]);
        float maxScale = std::max(std::max(scaleX, scaleY), scaleZ);
        
        radius = localRadius * maxScale;
        
        // Add small epsilon to avoid culling objects exactly on plane
        radius += 0.01f;
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
                              const std::map<std::string, std::vector<VkDescriptorSet>>* pPipelineDescriptorSets_ic,
                              RenderListBuilder::GetTextureDescriptorSetFunc getTextureDescriptorSet) {
    vecOutDrawCalls_out.clear();
    if ((pScene_ic == nullptr) || (pPipelineManager_ic == nullptr) || (pMaterialManager_ic == nullptr) || (pShaderManager_ic == nullptr))
        return;
    if ((pDevice_ic == VK_NULL_HANDLE) || (pRenderPass_ic == VK_NULL_HANDLE))
        return;

    const std::vector<Object>& vecObjects = pScene_ic->GetObjects();
    vecOutDrawCalls_out.reserve(vecObjects.size());
    std::vector<DrawCall> vecOpaque;
    std::vector<std::pair<float, DrawCall>> vecTransparent;
    vecOpaque.reserve(vecObjects.size());
    vecTransparent.reserve(vecObjects.size());

    // Extract frustum planes once per frame (if view-proj is provided)
    FrustumPlanes frustum;
    bool hasFrustum = false;
    if (pViewProj_ic != nullptr) {
        frustum.ExtractFromViewProj(pViewProj_ic);
        hasFrustum = true;
    }

    for (size_t objIndex = 0; objIndex < vecObjects.size(); ++objIndex) {
        const auto& obj = vecObjects[objIndex];
        if ((obj.pMaterial == nullptr) || (obj.pMesh == nullptr) || (obj.pMesh->HasValidBuffer() == false) || (obj.pushDataSize == 0u) || (obj.pushData.empty() == true))
            continue;
        const uint32_t lMaxPush = MaxPushConstantSize(obj.pMaterial->layoutDescriptor);
        if ((lMaxPush > 0u) && (obj.pushDataSize > lMaxPush))
            continue;
        
        float fDepthNdc = 0.0f;
        
        // Proper frustum culling with bounding sphere
        if (hasFrustum) {
            float cx, cy, cz, radius;
            ComputeWorldBoundingSphere(obj, cx, cy, cz, radius);
            
            // Test sphere against all frustum planes
            if (!frustum.IsSphereVisible(cx, cy, cz, radius)) {
                continue; // Completely outside frustum
            }
            
            // Compute depth for transparent sorting (use sphere center)
            float clipZ = pViewProj_ic[2]*cx + pViewProj_ic[6]*cy + pViewProj_ic[10]*cz + pViewProj_ic[14];
            float clipW = pViewProj_ic[3]*cx + pViewProj_ic[7]*cy + pViewProj_ic[11]*cz + pViewProj_ic[15];
            if (clipW > 0.0001f) {
                fDepthNdc = clipZ / clipW;
            }
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
            .dynamicOffsets     = {},  /* Not using dynamic offsets; SSBO indexed via push constant objectIndex */
            .pLocalTransform    = obj.localTransform,
            .color              = {obj.color[0], obj.color[1], obj.color[2], obj.color[3]},
            .objectIndex        = static_cast<uint32_t>(objIndex),
            .pipelineKey        = obj.pMaterial ? obj.pMaterial->pipelineKey : "",
        };
        
        // Use per-object texture descriptor set if available, otherwise fall back to pipeline default
        if (getTextureDescriptorSet && obj.pTexture && obj.pTexture->IsValid()) {
            // Pass all PBR textures: base color, metallic-roughness, emissive, normal, and occlusion
            VkDescriptorSet texDescSet = getTextureDescriptorSet(obj.pTexture, obj.pMetallicRoughnessTexture, obj.pEmissiveTexture, obj.pNormalTexture, obj.pOcclusionTexture);
            if (texDescSet != VK_NULL_HANDLE) {
                stD.descriptorSets = { texDescSet };
            }
        }
        
        // If no texture descriptor set, use pipeline default (main descriptor set with default texture)
        if (stD.descriptorSets.empty() && pPipelineDescriptorSets_ic != nullptr) {
            auto it = pPipelineDescriptorSets_ic->find(obj.pMaterial->pipelineKey);
            if (it != pPipelineDescriptorSets_ic->end() && !it->second.empty())
                stD.descriptorSets = it->second;
        }
        
        /* Skip draws that require descriptor sets but have none (e.g. main/wire before default texture is ready). */
        if (!obj.pMaterial->layoutDescriptor.descriptorSetLayouts.empty() && stD.descriptorSets.empty())
            continue;
        if (IsTransparentPipelineKey(obj.pMaterial->pipelineKey)) {
            vecTransparent.push_back({ fDepthNdc, stD });
        } else {
            vecOpaque.push_back(stD);
        }
    }

    std::sort(vecOpaque.begin(), vecOpaque.end(), DrawCallOrder);
    std::sort(vecTransparent.begin(), vecTransparent.end(),
        [](const std::pair<float, DrawCall>& a, const std::pair<float, DrawCall>& b) {
            return a.first > b.first; // back-to-front (farther first in Vulkan NDC depth)
        });

    vecOutDrawCalls_out = std::move(vecOpaque);
    vecOutDrawCalls_out.reserve(vecOutDrawCalls_out.size() + vecTransparent.size());
    for (auto& item : vecTransparent)
        vecOutDrawCalls_out.push_back(std::move(item.second));
}
