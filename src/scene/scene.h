#pragma once

#include "object.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>

/**
 * Scene change callback - notified when objects are added/removed/modified.
 */
using SceneChangeCallback = std::function<void()>;

/**
 * Scene: container for objects (and later lights, cameras). Cleared on unload so refs drop and managers can trim.
 * Tracks dirty state for efficient instanced rendering (only rebuild draw lists when needed).
 */
class Scene {
public:
    Scene() = default;
    explicit Scene(std::string name) : m_name(std::move(name)) {}

    const std::string& GetName() const { return m_name; }
    void SetName(std::string name) { m_name = std::move(name); }

    std::vector<Object>& GetObjects() { return m_objects; }
    const std::vector<Object>& GetObjects() const { return m_objects; }

    /** Drop all refs; managers can TrimUnused after this. */
    void Clear() {
        m_objects.clear();
        MarkDirty();
    }

    /** Update all objects with delta time. Calls each object's onUpdate callback if set. */
    void UpdateAllObjects(float deltaTime) {
        for (Object& obj : m_objects) {
            if (obj.onUpdate)
                obj.onUpdate(obj, deltaTime);
        }
    }

    /** Fill push data for all objects (viewProj * localTransform, color, objectIndex, camPos). Call each frame before building draw list. */
    void FillPushDataForAllObjects(const float* pViewProj_ic, const float* pCamPos_ic) {
        if (pViewProj_ic == nullptr)
            return;
        for (size_t i = 0; i < m_objects.size(); ++i)
            ObjectFillPushData(m_objects[i], pViewProj_ic, static_cast<uint32_t>(i), pCamPos_ic);
    }

    /* ---- Dirty Tracking ---- */
    
    /** Mark scene as dirty - draw list needs rebuilding. */
    void MarkDirty() { 
        m_bDirty = true;
        if (m_onChangeCallback) m_onChangeCallback();
    }
    
    /** Check if scene is dirty. */
    bool IsDirty() const { return m_bDirty; }
    
    /** Clear dirty flag. Called after draw list is rebuilt. */
    void ClearDirty() { m_bDirty = false; }
    
    /** Set callback for scene changes (for BatchedDrawList integration). */
    void SetChangeCallback(SceneChangeCallback callback) { m_onChangeCallback = std::move(callback); }
    
    /** Version number - increments on any structural change. */
    uint64_t GetVersion() const { return m_version; }
    
    /* ---- Object Management (with dirty tracking) ---- */
    
    /** Add object and mark dirty. Returns reference to added object. */
    Object& AddObject(Object obj) {
        m_objects.push_back(std::move(obj));
        MarkDirty();
        ++m_version;
        return m_objects.back();
    }
    
    /** Remove object at index and mark dirty. */
    void RemoveObject(size_t index) {
        if (index < m_objects.size()) {
            m_objects.erase(m_objects.begin() + static_cast<ptrdiff_t>(index));
            MarkDirty();
            ++m_version;
        }
    }
    
    /** Call when object's material/mesh/textures change (requires draw list rebuild). */
    void MarkObjectDirty(size_t index) {
        (void)index;
        MarkDirty();
        ++m_version;
    }
    
    /** Call when only transform/color changed (no draw list rebuild, just SSBO update). */
    void MarkObjectTransformDirty(size_t index) {
        (void)index;
        m_bTransformsDirty = true;
    }
    
    /** Check if any transforms changed. */
    bool AreTransformsDirty() const { return m_bTransformsDirty; }
    
    /** Clear transforms dirty flag. */
    void ClearTransformsDirty() { m_bTransformsDirty = false; }

private:
    std::string m_name;
    std::vector<Object> m_objects;
    bool m_bDirty = true;
    bool m_bTransformsDirty = false;
    uint64_t m_version = 0;
    SceneChangeCallback m_onChangeCallback;
};
