/*
 * TieredInstanceManager - Implementation
 */
#include "tiered_instance_manager.h"
#include "batched_draw_list.h"
#include "app/vulkan_app.h"

TierUpdateStats TieredInstanceManager::UpdateSSBO(
    ObjectData* pObjectData,
    uint32_t maxObjects,
    const std::vector<RenderObject>& renderObjects,
    const std::vector<DrawBatch>& opaqueBatches,
    const std::vector<DrawBatch>& transparentBatches,
    bool bSceneRebuilt
) {
    TierUpdateStats stats;
    if (!pObjectData || renderObjects.empty()) {
        m_lastStats = stats;
        return stats;
    }

    if (bSceneRebuilt) {
        m_rebuildFramesRemaining = kFramesInFlight;
    }
    bool bFullUpload = m_bForceFullUpload || (m_rebuildFramesRemaining > 0);
    m_bForceFullUpload = false;
    if (m_rebuildFramesRemaining > 0) {
        --m_rebuildFramesRemaining;
    }

    for (const auto& ro : renderObjects) {
        InstanceTier tier = static_cast<InstanceTier>(ro.instanceTier);
        switch (tier) {
            case InstanceTier::Static:      ++stats.staticCount; break;
            case InstanceTier::SemiStatic:  ++stats.semiStaticCount; break;
            case InstanceTier::Dynamic:     ++stats.dynamicCount; break;
            case InstanceTier::Procedural:  ++stats.proceduralCount; break;
        }
    }

    for (const auto& batch : opaqueBatches) {
        ProcessBatch(pObjectData, maxObjects, renderObjects, batch, bFullUpload, stats);
    }
    for (const auto& batch : transparentBatches) {
        ProcessBatch(pObjectData, maxObjects, renderObjects, batch, bFullUpload, stats);
    }
    m_lastStats = stats;
    return stats;
}

void TieredInstanceManager::ProcessBatch(
    ObjectData* pObjectData,
    uint32_t maxObjects,
    const std::vector<RenderObject>& renderObjects,
    const DrawBatch& batch,
    bool bFullUpload,
    TierUpdateStats& stats
) {
    const InstanceTier tier = batch.key.tier;
    uint32_t ssboOffset = batch.firstInstanceIndex;
    
    for (uint32_t objIdx : batch.objectIndices) {
        if (objIdx >= renderObjects.size() || ssboOffset >= maxObjects) continue;

        const RenderObject& ro = renderObjects[objIdx];
        
        bool needsUpload = false;
        
        switch (tier) {
            case InstanceTier::Static:    needsUpload = bFullUpload; if (needsUpload) ++stats.staticUploaded; break;
            case InstanceTier::SemiStatic: needsUpload = bFullUpload; if (needsUpload) ++stats.semiStaticUploaded; break;
            case InstanceTier::Dynamic:   needsUpload = true; ++stats.dynamicUploaded; break;
            case InstanceTier::Procedural: needsUpload = bFullUpload; if (needsUpload) ++stats.proceduralUploaded; break;
        }
        if (needsUpload)
            WriteObjectToSSBO(pObjectData[ssboOffset], ro);
        
        ++ssboOffset;
    }
}

void TieredInstanceManager::WriteObjectToSSBO(ObjectData& od, const RenderObject& ro) {
    const float* m = ro.worldMatrix;
    od.model = glm::mat4(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
                         m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
    od.emissive = glm::vec4(ro.emissive[0], ro.emissive[1], ro.emissive[2], ro.emissive[3]);
    float metallic = 0.f, roughness = 1.f, normalScale = 1.f, occlusionStrength = 1.f;
    if (ro.pRenderer) {
        metallic = ro.pRenderer->matProps.metallic;
        roughness = ro.pRenderer->matProps.roughness;
        normalScale = ro.pRenderer->matProps.normalScale;
        occlusionStrength = ro.pRenderer->matProps.occlusionStrength;
    }
    od.matProps = glm::vec4(metallic, roughness, normalScale, occlusionStrength);
    od.baseColor = glm::vec4(ro.color[0], ro.color[1], ro.color[2], ro.color[3]);
}
