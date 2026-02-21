/*
 * Scene — Scene container with component pools (Structure of Arrays).
 * Replaces the old scene.h with a proper ECS-style architecture.
 */
#pragma once

#include "gameobject.h"
#include "transform.h"
#include "renderer_component.h"
#include "light_component.h"
#include "camera_component.h"
#include <string>
#include <vector>
#include <unordered_map>

/**
 * SceneNew — Container for GameObjects and component pools.
 * Uses Structure of Arrays (SoA) for cache-efficient iteration.
 * 
 * Note: Named "SceneNew" during migration to avoid conflict with legacy Scene.
 * Will be renamed to "Scene" when migration is complete.
 */
class SceneNew {
public:
    SceneNew() = default;
    explicit SceneNew(const std::string& name) : m_name(name) {}

    /** Scene name. */
    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    /* ---- GameObject Management ---- */

    /** Create a new GameObject with a Transform. Returns the GameObject ID. */
    uint32_t CreateGameObject(const std::string& name = "");

    /** Destroy a GameObject by ID. Returns true if found and destroyed. */
    bool DestroyGameObject(uint32_t id);

    /** Find a GameObject by ID. Returns nullptr if not found. */
    GameObject* FindGameObject(uint32_t id);
    const GameObject* FindGameObject(uint32_t id) const;

    /** Find a GameObject by name. Returns nullptr if not found. */
    GameObject* FindGameObjectByName(const std::string& name);
    const GameObject* FindGameObjectByName(const std::string& name) const;

    /** Get all GameObjects (read-only). */
    const std::vector<GameObject>& GetGameObjects() const { return m_gameObjects; }

    /** Get all GameObjects (mutable). */
    std::vector<GameObject>& GetGameObjects() { return m_gameObjects; }

    /** Get number of GameObjects. */
    size_t GetGameObjectCount() const { return m_gameObjects.size(); }

    /* ---- Component Pool Accessors ---- */

    /** Get Transform pool. */
    const std::vector<Transform>& GetTransforms() const { return m_transforms; }
    std::vector<Transform>& GetTransforms() { return m_transforms; }

    /** Get Renderer pool. */
    const std::vector<RendererComponent>& GetRenderers() const { return m_renderers; }
    std::vector<RendererComponent>& GetRenderers() { return m_renderers; }

    /** Get Light pool. */
    const std::vector<LightComponent>& GetLights() const { return m_lights; }
    std::vector<LightComponent>& GetLights() { return m_lights; }

    /** Get Camera pool. */
    const std::vector<CameraComponent>& GetCameras() const { return m_cameras; }
    std::vector<CameraComponent>& GetCameras() { return m_cameras; }

    /* ---- Component Add/Remove ---- */

    /** Add a RendererComponent to a GameObject. Returns renderer index. */
    uint32_t AddRenderer(uint32_t gameObjectId, const RendererComponent& renderer);

    /** Add a LightComponent to a GameObject. Returns light index. */
    uint32_t AddLight(uint32_t gameObjectId, const LightComponent& light);

    /** Add a CameraComponent to a GameObject. Returns camera index. */
    uint32_t AddCamera(uint32_t gameObjectId, const CameraComponent& camera);

    /** Remove RendererComponent from a GameObject. */
    void RemoveRenderer(uint32_t gameObjectId);

    /** Remove LightComponent from a GameObject. */
    void RemoveLight(uint32_t gameObjectId);

    /** Remove CameraComponent from a GameObject. */
    void RemoveCamera(uint32_t gameObjectId);

    /* ---- Transform Helpers ---- */

    /** Get Transform for a GameObject. */
    Transform* GetTransform(uint32_t gameObjectId);
    const Transform* GetTransform(uint32_t gameObjectId) const;

    /** Update all dirty transform model matrices. Call before rendering. */
    void UpdateAllTransforms();

    /* ---- Scene Lifecycle ---- */

    /** Clear all GameObjects and components. */
    void Clear();

    /** Get next available GameObject ID. */
    uint32_t GetNextId() const { return m_nextId; }

    /* ---- Legacy Compatibility (Push Data for old pipeline) ---- */

    /** Fill push constant data for all renderers. For compatibility with existing RenderListBuilder. */
    void FillPushDataForAllObjects(const float* viewProj);

private:
    std::string m_name = "unnamed";
    uint32_t m_nextId = 1;

    // GameObjects (metadata + indices)
    std::vector<GameObject> m_gameObjects;
    std::unordered_map<uint32_t, size_t> m_idToIndex;

    // Component pools (Structure of Arrays)
    std::vector<Transform> m_transforms;
    std::vector<RendererComponent> m_renderers;
    std::vector<LightComponent> m_lights;
    std::vector<CameraComponent> m_cameras;

    // Free list indices for component reuse (future optimization)
    // std::vector<uint32_t> m_freeTransformIndices;
    // std::vector<uint32_t> m_freeRendererIndices;
    // std::vector<uint32_t> m_freeLightIndices;
};

