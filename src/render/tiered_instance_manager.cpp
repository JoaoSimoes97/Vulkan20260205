/*
 * TieredInstanceManager - Implementation
 */
#include "tiered_instance_manager.h"
#include "batched_draw_list.h"
#include "scene/scene.h"
#include "app/vulkan_app.h"  // For ObjectData struct

TierUpdateStats TieredInstanceManager::UpdateSSBO(
    ObjectData* pObjectData,
    uint32_t maxObjects,
    Scene* pScene,
    const std::vector<DrawBatch>& opaqueBatches,
    const std::vector<DrawBatch>& transparentBatches,
    bool bSceneRebuilt
) {
    TierUpdateStats stats;
    
    if (!pObjectData || !pScene) {
        m_lastStats = stats;
        return stats;
    }
    
    // Track rebuild across multiple frames for triple buffering.
    // When scene rebuilds, we need to upload static data to all 3 frame regions.
    if (bSceneRebuilt) {
        m_rebuildFramesRemaining = kFramesInFlight;
    }
    
    // Force full upload on first call, after invalidation, or while filling all frame buffers
    bool bFullUpload = m_bForceFullUpload || (m_rebuildFramesRemaining > 0);
    m_bForceFullUpload = false;
    
    // Decrement rebuild counter (if active)
    if (m_rebuildFramesRemaining > 0) {
        --m_rebuildFramesRemaining;
    }
    
    const auto& objects = pScene->GetObjects();
    
    // Count objects per tier
    for (const auto& obj : objects) {
        switch (obj.instanceTier) {
            case InstanceTier::Static:      ++stats.staticCount; break;
            case InstanceTier::SemiStatic:  ++stats.semiStaticCount; break;
            case InstanceTier::Dynamic:     ++stats.dynamicCount; break;
            case InstanceTier::Procedural:  ++stats.proceduralCount; break;
        }
    }
    
    // Process all batches
    for (const auto& batch : opaqueBatches) {
        ProcessBatch(pObjectData, maxObjects, objects, batch, bFullUpload, stats);
    }
    for (const auto& batch : transparentBatches) {
        ProcessBatch(pObjectData, maxObjects, objects, batch, bFullUpload, stats);
    }
    
    m_lastStats = stats;
    return stats;
}

void TieredInstanceManager::ProcessBatch(
    ObjectData* pObjectData,
    uint32_t maxObjects,
    const std::vector<Object>& objects,
    const DrawBatch& batch,
    bool bFullUpload,
    TierUpdateStats& stats
) {
    const InstanceTier tier = batch.key.tier;
    uint32_t ssboOffset = batch.firstInstanceIndex;
    
    for (uint32_t objIdx : batch.objectIndices) {
        if (objIdx >= objects.size() || ssboOffset >= maxObjects) continue;
        
        const Object& obj = objects[objIdx];
        
        bool needsUpload = false;
        
        switch (tier) {
            case InstanceTier::Static:
                // Only upload on full upload (scene rebuilt + all frames-in-flight)
                needsUpload = bFullUpload;
                if (needsUpload) {
                    ++stats.staticUploaded;
                    obj.ClearDirty();
                }
                break;
                
            case InstanceTier::SemiStatic:
                // Upload on full upload OR when object is dirty
                needsUpload = bFullUpload || obj.IsDirty();
                if (needsUpload) {
                    ++stats.semiStaticUploaded;
                    obj.ClearDirty();
                }
                break;
                
            case InstanceTier::Dynamic:
                // Always upload every frame
                needsUpload = true;
                ++stats.dynamicUploaded;
                break;
                
            case InstanceTier::Procedural:
                // Compute shader handles this - placeholder upload on full upload only
                needsUpload = bFullUpload;
                if (needsUpload) {
                    ++stats.proceduralUploaded;
                }
                break;
        }
        
        if (needsUpload) {
            WriteObjectToSSBO(pObjectData[ssboOffset], obj);
        }
        
        ++ssboOffset;
    }
}

void TieredInstanceManager::WriteObjectToSSBO(ObjectData& od, const Object& obj) {
    const float* m = obj.localTransform;
    od.model = glm::mat4(
        m[0], m[1], m[2], m[3],
        m[4], m[5], m[6], m[7],
        m[8], m[9], m[10], m[11],
        m[12], m[13], m[14], m[15]
    );
    od.emissive = glm::vec4(obj.emissive[0], obj.emissive[1], obj.emissive[2], obj.emissive[3]);
    od.matProps = glm::vec4(obj.metallicFactor, obj.roughnessFactor, obj.normalScale, obj.occlusionStrength);
    od.baseColor = glm::vec4(obj.color[0], obj.color[1], obj.color[2], obj.color[3]);
    // Reserved fields stay as-is (zeroed on allocation)
}
