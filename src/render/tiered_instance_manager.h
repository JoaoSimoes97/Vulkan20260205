/*
 * TieredInstanceManager - Professional tier-based SSBO update management.
 * 
 * Implements the 4-tier instancing system:
 * 
 * Tier 0 (Static):      GPU-resident, never moves. Written once on scene load.
 *                       Examples: terrain, buildings, static props.
 * 
 * Tier 1 (SemiStatic):  Dirty-flag updates. Written when object.bDirty is true.
 *                       Examples: doors, switches, destructibles.
 * 
 * Tier 2 (Dynamic):     Per-frame updates. Written every frame.
 *                       Examples: NPCs, physics objects, animated entities.
 * 
 * Tier 3 (Procedural):  Compute-generated. GPU fills SSBO directly.
 *                       Examples: particles, vegetation wind, cloth sim.
 * 
 * Benefits:
 * - Static objects: Zero CPU cost after initial upload
 * - SemiStatic: ~10% of objects update occasionally (doors open, etc.)
 * - Dynamic: Only NPCs/physics update every frame (~5% of scene)
 * - Procedural: Zero CPU cost - GPU computes positions
 */
#pragma once

#include "scene/object.h"
#include "scene/scene_unified.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <unordered_set>
#include <vector>

struct DrawBatch;

/**
 * Per-object GPU data (must match shader ObjectData struct).
 */
struct ObjectData;

/**
 * Statistics for tier-based updates.
 */
struct TierUpdateStats {
    uint32_t staticCount     = 0;  // Total static objects
    uint32_t semiStaticCount = 0;  // Total semi-static objects  
    uint32_t dynamicCount    = 0;  // Total dynamic objects
    uint32_t proceduralCount = 0;  // Total procedural objects
    
    uint32_t staticUploaded     = 0;  // Static objects uploaded this frame (only on rebuild)
    uint32_t semiStaticUploaded = 0;  // Semi-static objects uploaded this frame (dirty ones)
    uint32_t dynamicUploaded    = 0;  // Dynamic objects uploaded this frame (all of them)
    uint32_t proceduralUploaded = 0;  // Procedural objects updated (placeholder)
    
    /** Total objects uploaded this frame. */
    uint32_t TotalUploaded() const { 
        return staticUploaded + semiStaticUploaded + dynamicUploaded + proceduralUploaded; 
    }
    
    /** Total objects in scene. */
    uint32_t TotalObjects() const {
        return staticCount + semiStaticCount + dynamicCount + proceduralCount;
    }
    
    void Reset() {
        staticUploaded = semiStaticUploaded = dynamicUploaded = proceduralUploaded = 0;
    }
};

/**
 * TieredInstanceManager - Manages tier-based SSBO uploads.
 * 
 * Usage:
 * 1. Call UpdateSSBO() each frame with the mapped SSBO pointer
 * 2. Pass bSceneRebuilt=true when batches were rebuilt (triggers static upload)
 * 3. Dynamic objects always upload; SemiStatic only when dirty
 */
class TieredInstanceManager {
public:
    TieredInstanceManager() = default;
    
    /**
     * Update SSBO with object data from render list (unified Scene path).
     * @param bForceFullUploadThisFrame If true, upload all tiers this frame.
     * @param pMovedObjectIds If non-null (e.g. in editor), also upload Static/SemiStatic/Procedural objects whose gameObjectId is in this set, so only moved objects are re-uploaded.
     */
    TierUpdateStats UpdateSSBO(
        ObjectData* pObjectData,
        uint32_t maxObjects,
        const std::vector<RenderObject>& renderObjects,
        const std::vector<DrawBatch>& opaqueBatches,
        const std::vector<DrawBatch>& transparentBatches,
        bool bSceneRebuilt,
        bool bForceFullUploadThisFrame = false,
        const std::unordered_set<uint32_t>* pMovedObjectIds = nullptr
    );
    
    /**
     * Get last frame's update statistics.
     */
    const TierUpdateStats& GetLastStats() const { return m_lastStats; }
    
    /**
     * Force all objects to re-upload on next frame (e.g., after GPU memory reallocation).
     */
    void InvalidateAll() { m_bForceFullUpload = true; }
    
private:
    void WriteObjectToSSBO(ObjectData& od, const RenderObject& ro);

    void ProcessBatch(
        ObjectData* pObjectData,
        uint32_t maxObjects,
        const std::vector<RenderObject>& renderObjects,
        const DrawBatch& batch,
        bool bFullUpload,
        const std::unordered_set<uint32_t>* pMovedObjectIds,
        TierUpdateStats& stats
    );
    
    TierUpdateStats m_lastStats;
    bool m_bForceFullUpload = true;  // First frame needs full upload
    
    /** Frames remaining that need full upload after scene rebuild.
        With triple buffering, we need to upload to all 3 frame regions. */
    uint32_t m_rebuildFramesRemaining = 3;  // Start at max to ensure first frames are filled
    static constexpr uint32_t kFramesInFlight = 3;
};
