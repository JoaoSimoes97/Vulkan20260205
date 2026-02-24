/**
 * SceneUnified — Unified ECS scene with render-optimized data layout.
 *
 * Combines the best of both systems:
 * - ECS component pools from SceneNew (cache-efficient iteration)
 * - Render-ready object data from Scene (GPU upload optimization)
 *
 * Key design decisions:
 * 1. GameObjects are lightweight handles (ID + component indices)
 * 2. Components are stored in Structure of Arrays (SoA) pools
 * 3. Render data is derived on-demand, no sync step needed
 * 4. Dirty flags track what needs GPU update
 *
 * Phase 4.2: Unified Scene System
 */

#pragma once

#include "transform.h"
#include "renderer_component.h"
#include "light_component.h"
#include "camera_component.h"
#include "component.h"
#include "gameobject.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
struct MaterialHandle;
class MeshHandle;
class TextureHandle;
struct AABB;
struct BoundingSphere;

/**
 * RenderObject — Render-time view of a renderable entity.
 *
 * This is NOT stored in the scene — it's computed on-demand from components.
 * Used by BatchedDrawList to build draw batches without copying data.
 */
struct RenderObject {
    // Component references (non-owning)
    const Transform*            pTransform      = nullptr;
    const RendererComponent*    pRenderer       = nullptr;
    
    // Resolved resource handles (from RendererComponent)
    std::shared_ptr<MeshHandle>     mesh;
    std::shared_ptr<MaterialHandle> material;
    std::shared_ptr<TextureHandle>  texture;  // Base color texture
    
    // Cached world transform (computed from Transform hierarchy)
    float worldMatrix[16];
    
    // World-space bounding sphere (for frustum culling)
    float boundsCenterX = 0.f, boundsCenterY = 0.f, boundsCenterZ = 0.f;
    float boundsRadius = 0.f;
    
    // GameObject ID (for editor selection, etc.)
    uint32_t gameObjectId = 0;
    
    // Index into ObjectData SSBO
    uint32_t objectIndex = 0;
};

/**
 * SceneDirtyFlags — Tracks what needs updating.
 */
enum class SceneDirtyFlags : uint32_t {
    None            = 0,
    Transforms      = 1 << 0,   // Transform hierarchy changed
    Renderers       = 1 << 1,   // Renderer components added/removed/changed
    Lights          = 1 << 2,   // Light components changed
    Cameras         = 1 << 3,   // Camera components changed
    Structure       = 1 << 4,   // GameObjects added/removed
    All             = 0xFFFFFFFF
};

inline SceneDirtyFlags operator|(SceneDirtyFlags a, SceneDirtyFlags b) {
    return static_cast<SceneDirtyFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline SceneDirtyFlags operator&(SceneDirtyFlags a, SceneDirtyFlags b) {
    return static_cast<SceneDirtyFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool HasFlag(SceneDirtyFlags flags, SceneDirtyFlags flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

/**
 * Scene change callback type.
 */
using SceneChangeCallback = std::function<void()>;

/**
 * Scene — Unified ECS scene container.
 *
 * Usage:
 *   Scene scene("MainScene");
 *   uint32_t id = scene.CreateGameObject("Player");
 *   scene.AddTransform(id, Transform(...));
 *   scene.AddRenderer(id, RendererComponent(...));
 *   
 *   // Each frame:
 *   scene.UpdateTransformHierarchy();
 *   auto renderObjects = scene.BuildRenderList();
 *   // ... upload to GPU and draw ...
 */
class Scene {
public:
    Scene() = default;
    explicit Scene(const std::string& name) : m_name(name) {}
    ~Scene() = default;

    // Non-copyable (owns resources)
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Movable
    Scene(Scene&&) = default;
    Scene& operator=(Scene&&) = default;

    /* ======== Scene Properties ======== */

    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    /* ======== GameObject Management ======== */

    /**
     * Create a new GameObject. Returns the unique ID.
     * @param name Optional name for editor display
     * @return GameObject ID
     */
    uint32_t CreateGameObject(const std::string& name = "");

    /**
     * Destroy a GameObject and all its components.
     * @param id GameObject ID
     * @return true if found and destroyed
     */
    bool DestroyGameObject(uint32_t id);

    /**
     * Find a GameObject by ID.
     * @return Pointer to GameObject, or nullptr if not found
     */
    GameObject* FindGameObject(uint32_t id);
    const GameObject* FindGameObject(uint32_t id) const;

    /**
     * Find a GameObject by name (first match).
     * @return Pointer to GameObject, or nullptr if not found
     */
    GameObject* FindGameObjectByName(const std::string& name);
    const GameObject* FindGameObjectByName(const std::string& name) const;

    /**
     * Get all GameObjects (read-only).
     */
    const std::vector<GameObject>& GetGameObjects() const { return m_gameObjects; }

    /**
     * Get all GameObjects (mutable).
     */
    std::vector<GameObject>& GetGameObjects() { return m_gameObjects; }

    /**
     * Get number of GameObjects.
     */
    size_t GetGameObjectCount() const { return m_gameObjects.size(); }

    /* ======== Component Pool Accessors ======== */

    // Transform pool
    const std::vector<Transform>& GetTransforms() const { return m_transforms; }
    std::vector<Transform>& GetTransforms() { return m_transforms; }

    // Renderer pool
    const std::vector<RendererComponent>& GetRenderers() const { return m_renderers; }
    std::vector<RendererComponent>& GetRenderers() { return m_renderers; }

    // Light pool
    const std::vector<LightComponent>& GetLights() const { return m_lights; }
    std::vector<LightComponent>& GetLights() { return m_lights; }

    // Camera pool
    const std::vector<CameraComponent>& GetCameras() const { return m_cameras; }
    std::vector<CameraComponent>& GetCameras() { return m_cameras; }

    /* ======== Component Add/Remove ======== */

    /** Add a Transform to a GameObject. Returns component index. */
    uint32_t AddTransform(uint32_t gameObjectId, const Transform& transform);

    /** Add a RendererComponent to a GameObject. Returns component index. */
    uint32_t AddRenderer(uint32_t gameObjectId, const RendererComponent& renderer);

    /** Add a LightComponent to a GameObject. Returns component index. */
    uint32_t AddLight(uint32_t gameObjectId, const LightComponent& light);

    /** Add a CameraComponent to a GameObject. Returns component index. */
    uint32_t AddCamera(uint32_t gameObjectId, const CameraComponent& camera);

    /** Get Transform for a GameObject. Returns nullptr if not found. */
    Transform* GetTransform(uint32_t gameObjectId);
    const Transform* GetTransform(uint32_t gameObjectId) const;

    /** Get RendererComponent for a GameObject. Returns nullptr if not found. */
    RendererComponent* GetRenderer(uint32_t gameObjectId);
    const RendererComponent* GetRenderer(uint32_t gameObjectId) const;

    /** Get LightComponent for a GameObject. Returns nullptr if not found. */
    LightComponent* GetLight(uint32_t gameObjectId);
    const LightComponent* GetLight(uint32_t gameObjectId) const;

    /** Get CameraComponent for a GameObject. Returns nullptr if not found. */
    CameraComponent* GetCamera(uint32_t gameObjectId);
    const CameraComponent* GetCamera(uint32_t gameObjectId) const;

    /* ======== Transform Hierarchy ======== */

    /**
     * Update all transform matrices.
     * Call once per frame before rendering.
     */
    void UpdateTransformHierarchy();

    /* ======== Render List Building ======== */

    /**
     * Build render list from scene.
     * Returns a list of RenderObjects ready for batching.
     * Optionally performs frustum culling.
     *
     * @param viewProj View-projection matrix (16 floats, column-major)
     * @param frustumCull If true, cull objects outside frustum
     * @param outCulledCount Optional output: number of objects culled
     * @return Vector of RenderObjects
     */
    std::vector<RenderObject> BuildRenderList(const float* viewProj = nullptr,
                                               bool frustumCull = true,
                                               uint32_t* outCulledCount = nullptr) const;

    /**
     * Get count of renderable objects (GameObjects with RendererComponent).
     */
    uint32_t GetRenderableCount() const;

    /* ======== Dirty Tracking ======== */

    /** Mark specific dirty flags. */
    void MarkDirty(SceneDirtyFlags flags) { m_dirtyFlags = m_dirtyFlags | flags; }

    /** Clear specific dirty flags. */
    void ClearDirty(SceneDirtyFlags flags) {
        m_dirtyFlags = static_cast<SceneDirtyFlags>(
            static_cast<uint32_t>(m_dirtyFlags) & ~static_cast<uint32_t>(flags));
    }

    /** Check if any dirty flag is set. */
    bool IsDirty() const { return m_dirtyFlags != SceneDirtyFlags::None; }

    /** Check specific dirty flag. */
    bool IsDirty(SceneDirtyFlags flag) const { return HasFlag(m_dirtyFlags, flag); }

    /** Get all dirty flags. */
    SceneDirtyFlags GetDirtyFlags() const { return m_dirtyFlags; }

    /** Get scene version (incremented on structural changes). */
    uint32_t GetVersion() const { return m_version; }

    /* ======== Change Callbacks ======== */

    /** Set callback for scene changes (called after any modification). */
    void SetOnChangeCallback(SceneChangeCallback callback) { m_onChangeCallback = std::move(callback); }

private:
    void NotifyChange() {
        ++m_version;
        if (m_onChangeCallback) {
            m_onChangeCallback();
        }
    }

    // Scene properties
    std::string m_name;

    // GameObjects
    std::vector<GameObject> m_gameObjects;
    std::unordered_map<uint32_t, size_t> m_idToIndex;
    uint32_t m_nextId = 1;

    // Component pools (Structure of Arrays)
    std::vector<Transform>          m_transforms;
    std::vector<RendererComponent>  m_renderers;
    std::vector<LightComponent>     m_lights;
    std::vector<CameraComponent>    m_cameras;

    // Maps: gameObjectId -> component index in pool
    std::unordered_map<uint32_t, uint32_t> m_transformMap;
    std::unordered_map<uint32_t, uint32_t> m_rendererMap;
    std::unordered_map<uint32_t, uint32_t> m_lightMap;
    std::unordered_map<uint32_t, uint32_t> m_cameraMap;

    // Dirty tracking
    SceneDirtyFlags m_dirtyFlags = SceneDirtyFlags::None;
    uint32_t m_version = 0;

    // Change callback
    SceneChangeCallback m_onChangeCallback;
};
