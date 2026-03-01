/*
 * BatchedDrawList - Efficient instanced rendering with dirty tracking.
 * 
 * Groups objects by (mesh, material, textures) into batches.
 * Each batch = 1 draw call with instanceCount = N objects.
 * Uses gl_InstanceIndex + batchStartIndex to index into ObjectData SSBO.
 * 
 * Only rebuilds when scene changes (dirty flag), not every frame.
 */
#pragma once

#include "vulkan/vulkan_command_buffers.h"
#include "scene/object.h"
#include "scene/scene_unified.h"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <tuple>
class MaterialManager;
class MeshManager;
class PipelineManager;
class VulkanShaderManager;
class TextureHandle;
class MeshHandle;
struct MaterialHandle;

/**
 * Key for batching: objects with same key can be drawn in one instanced call.
 * Includes instanceTier to keep tiers separate (different update patterns).
 */
struct BatchKey {
    std::shared_ptr<MeshHandle> mesh;
    std::shared_ptr<MaterialHandle> material;
    std::shared_ptr<TextureHandle> baseColorTexture;
    std::shared_ptr<TextureHandle> metallicRoughnessTexture;
    std::shared_ptr<TextureHandle> emissiveTexture;
    std::shared_ptr<TextureHandle> normalTexture;
    std::shared_ptr<TextureHandle> occlusionTexture;
    InstanceTier tier = InstanceTier::Static;  // Objects batch only with same tier
    
    bool operator<(const BatchKey& other) const {
        return std::tie(mesh, material, baseColorTexture, metallicRoughnessTexture, 
                        emissiveTexture, normalTexture, occlusionTexture, tier) <
               std::tie(other.mesh, other.material, other.baseColorTexture, other.metallicRoughnessTexture,
                        other.emissiveTexture, other.normalTexture, other.occlusionTexture, other.tier);
    }
    
    bool operator==(const BatchKey& other) const {
        return mesh == other.mesh && material == other.material &&
               baseColorTexture == other.baseColorTexture &&
               metallicRoughnessTexture == other.metallicRoughnessTexture &&
               emissiveTexture == other.emissiveTexture &&
               normalTexture == other.normalTexture &&
               occlusionTexture == other.occlusionTexture &&
               tier == other.tier;
    }
};

/**
 * A batch of objects sharing the same mesh/material/textures.
 */
struct DrawBatch {
    BatchKey key;
    std::vector<uint32_t> objectIndices;  // Indices into scene objects array
    
    // Cached Vulkan handles (resolved from key)
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceSize vertexBufferOffset = 0;
    uint32_t vertexCount = 0;
    uint32_t firstVertex = 0;
    std::vector<VkDescriptorSet> descriptorSets;
    std::string pipelineKey;
    
    // First object index for gl_InstanceIndex offset
    uint32_t firstInstanceIndex = 0;
    
    // Dominant tier for this batch (tier with most objects)
    InstanceTier dominantTier = InstanceTier::Static;
};

/**
 * Instanced push constants - shared per draw call, not per object.
 * Objects indexed via gl_InstanceIndex + batchStartIndex into ObjectData SSBO.
 */
struct InstancedPushConstants {
    float viewProj[16];         // 64 bytes - shared viewProj matrix
    float camPos[4];            // 16 bytes - camera world position
    uint32_t batchStartIndex;   // 4 bytes - first objectIndex for this batch
    uint32_t _pad[3];           // 12 bytes - padding to 96 bytes total
};
static_assert(sizeof(InstancedPushConstants) == 96, "InstancedPushConstants must be 96 bytes");

/**
 * BatchedDrawList - Builds and caches instanced draw batches.
 * 
 * Usage:
 * 1. Call SetDirty() when scene changes (add/remove/modify objects)
 * 2. Call RebuildIfDirty() once per frame (no-op if not dirty)
 * 3. Use GetBatches() to iterate and draw
 */
class BatchedDrawList {
public:
    BatchedDrawList() = default;
    
    /**
     * Callback to get/create descriptor set for textures.
     */
    using GetTextureDescriptorSetFunc = std::function<VkDescriptorSet(
        std::shared_ptr<TextureHandle>, std::shared_ptr<TextureHandle>,
        std::shared_ptr<TextureHandle>, std::shared_ptr<TextureHandle>,
        std::shared_ptr<TextureHandle>)>;
    
    /**
     * Mark list as dirty - will rebuild on next RebuildIfDirty().
     */
    void SetDirty() { m_bDirty = true; }
    
    /**
     * Check if list needs rebuilding.
     */
    bool IsDirty() const { return m_bDirty; }
    
    /**
     * Rebuild batches if dirty. Uses scene->BuildRenderList() (unified Scene).
     * Returns true if rebuild occurred.
     */
    bool RebuildIfDirty(
        const Scene* pScene,
        VkDevice device,
        VkRenderPass renderPass,
        bool hasDepth,
        PipelineManager* pPipelineManager,
        MaterialManager* pMaterialManager,
        VulkanShaderManager* pShaderManager,
        const std::map<std::string, std::vector<VkDescriptorSet>>* pPipelineDescriptorSets,
        GetTextureDescriptorSetFunc getTextureDescriptorSet = nullptr
    );
    
    /**
     * Get opaque batches (sorted by pipeline/mesh for minimal state changes).
     */
    const std::vector<DrawBatch>& GetOpaqueBatches() const { return m_opaqueBatches; }
    
    /**
     * Get transparent batches (will be sorted by depth each frame).
     */
    const std::vector<DrawBatch>& GetTransparentBatches() const { return m_transparentBatches; }
    
    /**
     * Get all object indices that passed frustum culling (for SSBO upload).
     * Indices are in batch order: [batch0.objects..., batch1.objects..., ...]
     */
    const std::vector<uint32_t>& GetVisibleObjectIndices() const { return m_visibleObjectIndices; }

    /** Last render list from BuildRenderList (for SSBO upload / GPU culler). Call after RebuildIfDirty. */
    const std::vector<RenderObject>& GetLastRenderObjects() const { return m_lastRenderObjects; }
    
    /**
     * Get the batch for a given object index. Returns nullptr if not found.
     */
    const DrawBatch* GetBatchForObject(uint32_t objIdx) const {
        auto it = m_objToBatchIdxOpaque.find(objIdx);
        if (it != m_objToBatchIdxOpaque.end() && it->second < m_opaqueBatches.size()) {
            return &m_opaqueBatches[it->second];
        }
        auto it2 = m_objToBatchIdxTransparent.find(objIdx);
        if (it2 != m_objToBatchIdxTransparent.end() && it2->second < m_transparentBatches.size()) {
            return &m_transparentBatches[it2->second];
        }
        return nullptr;
    }
    
    /**
     * Get total draw call count (sum of all batches).
     */
    size_t GetDrawCallCount() const { return m_opaqueBatches.size() + m_transparentBatches.size(); }
    
    /**
     * Get total instance count (sum of all instances across batches).
     */
    size_t GetTotalInstanceCount() const;
    
    /**
     * Update visible objects based on frustum culling.
     * Uses last built render list (m_lastRenderObjects). Call after RebuildIfDirty.
     */
    size_t UpdateVisibility(const float* pViewProj, const Scene* pScene);
    
    /**
     * Clear all batches.
     */
    void Clear();
    
private:
    void BuildBatches(
        const std::vector<RenderObject>& renderObjects,
        VkDevice device,
        VkRenderPass renderPass,
        bool hasDepth,
        PipelineManager* pPipelineManager,
        MaterialManager* pMaterialManager,
        VulkanShaderManager* pShaderManager,
        const std::map<std::string, std::vector<VkDescriptorSet>>* pPipelineDescriptorSets,
        GetTextureDescriptorSetFunc getTextureDescriptorSet
    );

    void SortBatches();

    bool m_bDirty = true;
    std::vector<DrawBatch> m_opaqueBatches;
    std::vector<DrawBatch> m_transparentBatches;
    std::vector<uint32_t> m_visibleObjectIndices;

    std::map<uint32_t, size_t> m_objToBatchIdxOpaque;
    std::map<uint32_t, size_t> m_objToBatchIdxTransparent;

    const Scene* m_pLastScene = nullptr;
    size_t m_lastObjectCount = 0;
    std::vector<RenderObject> m_lastRenderObjects;
};
