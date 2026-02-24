/*
 * BatchedDrawList - Efficient instanced rendering implementation.
 */
#include "batched_draw_list.h"
#include "managers/material_manager.h"
#include "managers/mesh_manager.h"
#include "managers/pipeline_manager.h"
#include "managers/texture_manager.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "vulkan/vulkan_shader_manager.h"
#include "vulkan/vulkan_utils.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace {
    /**
     * Frustum planes for visibility culling.
     */
    struct FrustumPlanes {
        float planes[6][4];
        
        void ExtractFromViewProj(const float* vp) {
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
            // Near: row3 + row2
            planes[4][0] = vp[3] + vp[2];
            planes[4][1] = vp[7] + vp[6];
            planes[4][2] = vp[11] + vp[10];
            planes[4][3] = vp[15] + vp[14];
            // Far: row3 - row2
            planes[5][0] = vp[3] - vp[2];
            planes[5][1] = vp[7] - vp[6];
            planes[5][2] = vp[11] - vp[10];
            planes[5][3] = vp[15] - vp[14];
            
            // Normalize planes
            for (int i = 0; i < 6; ++i) {
                float len = std::sqrt(planes[i][0]*planes[i][0] + planes[i][1]*planes[i][1] + planes[i][2]*planes[i][2]);
                if (len > 0.0001f) {
                    float invLen = 1.0f / len;
                    for (int j = 0; j < 4; ++j) planes[i][j] *= invLen;
                }
            }
        }
        
        bool IsSphereVisible(float cx, float cy, float cz, float radius) const {
            for (int i = 0; i < 6; ++i) {
                float dist = planes[i][0]*cx + planes[i][1]*cy + planes[i][2]*cz + planes[i][3];
                if (dist < -radius) return false;
            }
            return true;
        }
    };
    
    void ComputeWorldBoundingSphere(const Object& obj, float& cx, float& cy, float& cz, float& radius) {
        const float* m = obj.localTransform;
        cx = m[12]; cy = m[13]; cz = m[14];
        
        if (obj.pMesh && obj.pMesh->GetAABB().IsValid()) {
            const MeshAABB& aabb = obj.pMesh->GetAABB();
            float localCx, localCy, localCz;
            aabb.GetCenter(localCx, localCy, localCz);
            cx = m[0]*localCx + m[4]*localCy + m[8]*localCz + m[12];
            cy = m[1]*localCx + m[5]*localCy + m[9]*localCz + m[13];
            cz = m[2]*localCx + m[6]*localCy + m[10]*localCz + m[14];
            
            float scaleX = std::sqrt(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
            float scaleY = std::sqrt(m[4]*m[4] + m[5]*m[5] + m[6]*m[6]);
            float scaleZ = std::sqrt(m[8]*m[8] + m[9]*m[9] + m[10]*m[10]);
            radius = aabb.GetBoundingSphereRadius() * std::max({scaleX, scaleY, scaleZ}) + 0.01f;
        } else {
            radius = 2.0f;
        }
    }
    
    bool IsTransparentPipelineKey(const std::string& key) {
        return key.find("transparent") != std::string::npos;
    }
    
    uint32_t MaxPushConstantSize(const PipelineLayoutDescriptor& layout) {
        uint32_t maxEnd = 0;
        for (const auto& r : layout.pushConstantRanges)
            maxEnd = std::max(maxEnd, r.offset + r.size);
        return maxEnd;
    }
    
    // Sort batches by pipeline/mesh to minimize state changes
    bool BatchOrder(const DrawBatch& a, const DrawBatch& b) {
        if (a.pipeline != b.pipeline) return a.pipeline < b.pipeline;
        if (a.vertexBuffer != b.vertexBuffer) return a.vertexBuffer < b.vertexBuffer;
        return a.vertexCount < b.vertexCount;
    }
}

size_t BatchedDrawList::GetTotalInstanceCount() const {
    size_t total = 0;
    for (const auto& batch : m_opaqueBatches) total += batch.objectIndices.size();
    for (const auto& batch : m_transparentBatches) total += batch.objectIndices.size();
    return total;
}

void BatchedDrawList::Clear() {
    m_opaqueBatches.clear();
    m_transparentBatches.clear();
    m_visibleObjectIndices.clear();
    m_bDirty = true;
}

bool BatchedDrawList::RebuildIfDirty(
    const Scene* pScene,
    VkDevice device,
    VkRenderPass renderPass,
    bool hasDepth,
    PipelineManager* pPipelineManager,
    MaterialManager* pMaterialManager,
    VulkanShaderManager* pShaderManager,
    const std::map<std::string, std::vector<VkDescriptorSet>>* pPipelineDescriptorSets,
    GetTextureDescriptorSetFunc getTextureDescriptorSet
) {
    // Check for automatic dirty detection
    if (pScene != m_pLastScene || (pScene && pScene->GetObjects().size() != m_lastObjectCount)) {
        m_bDirty = true;
    }
    
    if (!m_bDirty) return false;
    
    BuildBatches(pScene, device, renderPass, hasDepth, pPipelineManager, 
                 pMaterialManager, pShaderManager, pPipelineDescriptorSets, getTextureDescriptorSet);
    
    m_pLastScene = pScene;
    m_lastObjectCount = pScene ? pScene->GetObjects().size() : 0;
    m_bDirty = false;
    
    VulkanUtils::LogTrace("BatchedDrawList rebuilt: {} opaque batches, {} transparent batches, {} total instances",
        m_opaqueBatches.size(), m_transparentBatches.size(), GetTotalInstanceCount());
    
    return true;
}

void BatchedDrawList::BuildBatches(
    const Scene* pScene,
    VkDevice device,
    VkRenderPass renderPass,
    bool hasDepth,
    PipelineManager* pPipelineManager,
    MaterialManager* pMaterialManager,
    VulkanShaderManager* pShaderManager,
    const std::map<std::string, std::vector<VkDescriptorSet>>* pPipelineDescriptorSets,
    GetTextureDescriptorSetFunc getTextureDescriptorSet
) {
    m_opaqueBatches.clear();
    m_transparentBatches.clear();
    m_visibleObjectIndices.clear();
    
    if (!pScene || !pPipelineManager || !pMaterialManager || !pShaderManager) return;
    if (device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE) return;
    
    const auto& objects = pScene->GetObjects();
    if (objects.empty()) return;
    
    // Group objects by BatchKey
    std::map<BatchKey, std::vector<uint32_t>> batchGroups;
    
    for (size_t i = 0; i < objects.size(); ++i) {
        const auto& obj = objects[i];
        if (!obj.pMaterial || !obj.pMesh || !obj.pMesh->HasValidBuffer()) continue;
        
        BatchKey key;
        key.mesh = obj.pMesh;
        key.material = obj.pMaterial;
        key.baseColorTexture = obj.pTexture;
        key.metallicRoughnessTexture = obj.pMetallicRoughnessTexture;
        key.emissiveTexture = obj.pEmissiveTexture;
        key.normalTexture = obj.pNormalTexture;
        key.occlusionTexture = obj.pOcclusionTexture;
        
        batchGroups[key].push_back(static_cast<uint32_t>(i));
    }
    
    // Convert groups to DrawBatch structs
    uint32_t globalInstanceOffset = 0;
    
    for (auto& [key, indices] : batchGroups) {
        DrawBatch batch;
        batch.key = key;
        batch.objectIndices = std::move(indices);
        batch.firstInstanceIndex = globalInstanceOffset;
        globalInstanceOffset += static_cast<uint32_t>(batch.objectIndices.size());
        
        // Resolve Vulkan handles from key
        if (!key.mesh || !key.material) continue;
        
        batch.pipeline = key.material->GetPipelineIfReady(device, renderPass, pPipelineManager, pShaderManager, hasDepth);
        batch.pipelineLayout = key.material->GetPipelineLayoutIfReady(pPipelineManager);
        
        if (batch.pipeline == VK_NULL_HANDLE || batch.pipelineLayout == VK_NULL_HANDLE) continue;
        
        batch.vertexBuffer = key.mesh->GetVertexBuffer();
        batch.vertexBufferOffset = key.mesh->GetVertexBufferOffset();
        batch.vertexCount = key.mesh->GetVertexCount();
        batch.firstVertex = key.mesh->GetFirstVertex();
        batch.pipelineKey = key.material->pipelineKey;
        
        if (batch.vertexBuffer == VK_NULL_HANDLE || batch.vertexCount == 0) continue;
        
        // Get descriptor set for textures
        if (getTextureDescriptorSet && key.baseColorTexture && key.baseColorTexture->IsValid()) {
            VkDescriptorSet texDescSet = getTextureDescriptorSet(
                key.baseColorTexture, key.metallicRoughnessTexture,
                key.emissiveTexture, key.normalTexture, key.occlusionTexture);
            if (texDescSet != VK_NULL_HANDLE) {
                batch.descriptorSets = { texDescSet };
            }
        }
        
        // Fallback to pipeline default descriptor sets
        if (batch.descriptorSets.empty() && pPipelineDescriptorSets) {
            auto it = pPipelineDescriptorSets->find(batch.pipelineKey);
            if (it != pPipelineDescriptorSets->end() && !it->second.empty()) {
                batch.descriptorSets = it->second;
            }
        }
        
        // Skip if descriptor sets required but not available
        if (!key.material->layoutDescriptor.descriptorSetLayouts.empty() && batch.descriptorSets.empty()) {
            continue;
        }
        
        // Add to appropriate list
        if (IsTransparentPipelineKey(batch.pipelineKey)) {
            m_transparentBatches.push_back(std::move(batch));
        } else {
            m_opaqueBatches.push_back(std::move(batch));
        }
    }
    
    // Sort opaque batches for minimal state changes
    SortBatches();
    
    // Build object-to-batch lookup maps
    m_objToBatchIdxOpaque.clear();
    m_objToBatchIdxTransparent.clear();
    for (size_t batchIdx = 0; batchIdx < m_opaqueBatches.size(); ++batchIdx) {
        for (uint32_t objIdx : m_opaqueBatches[batchIdx].objectIndices) {
            m_objToBatchIdxOpaque[objIdx] = batchIdx;
        }
    }
    for (size_t batchIdx = 0; batchIdx < m_transparentBatches.size(); ++batchIdx) {
        for (uint32_t objIdx : m_transparentBatches[batchIdx].objectIndices) {
            m_objToBatchIdxTransparent[objIdx] = batchIdx;
        }
    }
    
    // Build visible object indices (initially all objects in batch order)
    m_visibleObjectIndices.reserve(globalInstanceOffset);
    for (const auto& batch : m_opaqueBatches) {
        for (uint32_t idx : batch.objectIndices) {
            m_visibleObjectIndices.push_back(idx);
        }
    }
    for (const auto& batch : m_transparentBatches) {
        for (uint32_t idx : batch.objectIndices) {
            m_visibleObjectIndices.push_back(idx);
        }
    }
}

void BatchedDrawList::SortBatches() {
    std::sort(m_opaqueBatches.begin(), m_opaqueBatches.end(), BatchOrder);
    // Transparent batches sorted by depth in UpdateVisibility()
}

size_t BatchedDrawList::UpdateVisibility(const float* pViewProj, const Scene* pScene) {
    if (!pViewProj || !pScene) {
        // No frustum - all visible
        m_visibleObjectIndices.clear();
        for (const auto& batch : m_opaqueBatches) {
            for (uint32_t objIdx : batch.objectIndices) {
                m_visibleObjectIndices.push_back(objIdx);
            }
        }
        for (const auto& batch : m_transparentBatches) {
            for (uint32_t objIdx : batch.objectIndices) {
                m_visibleObjectIndices.push_back(objIdx);
            }
        }
        return m_visibleObjectIndices.size();
    }
    
    FrustumPlanes frustum;
    frustum.ExtractFromViewProj(pViewProj);
    
    const auto& objects = pScene->GetObjects();
    m_visibleObjectIndices.clear();
    m_visibleObjectIndices.reserve(objects.size());
    
    // Collect visible objects from all batches WITHOUT modifying the batches.
    // The batches keep ALL objects; visibility is per-frame.
    for (const auto& batch : m_opaqueBatches) {
        for (uint32_t objIdx : batch.objectIndices) {
            if (objIdx >= objects.size()) continue;
            
            const auto& obj = objects[objIdx];
            float cx, cy, cz, radius;
            ComputeWorldBoundingSphere(obj, cx, cy, cz, radius);
            
            if (frustum.IsSphereVisible(cx, cy, cz, radius)) {
                m_visibleObjectIndices.push_back(objIdx);
            }
        }
    }
    
    for (const auto& batch : m_transparentBatches) {
        for (uint32_t objIdx : batch.objectIndices) {
            if (objIdx >= objects.size()) continue;
            
            const auto& obj = objects[objIdx];
            float cx, cy, cz, radius;
            ComputeWorldBoundingSphere(obj, cx, cy, cz, radius);
            
            if (frustum.IsSphereVisible(cx, cy, cz, radius)) {
                m_visibleObjectIndices.push_back(objIdx);
            }
        }
    }
    
    return m_visibleObjectIndices.size();
}
