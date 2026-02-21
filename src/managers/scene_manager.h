#pragma once

#include "gltf_loader.h"
#include "scene/scene.h"
#include "core/scene_new.h"
#include <nlohmann/json_fwd.hpp>
#include <memory>
#include <string>
#include <map>

namespace tinygltf {
class Model;
struct Primitive;
}

class MaterialManager;
class MeshManager;
class TextureManager;

/**
 * SceneManager: owns current scene and level loading. Level = JSON descriptor + many glTFs (one per instance).
 * SetDependencies() must be called before LoadLevelFromFile or LoadDefaultLevelOrCreate.
 * Supports procedural meshes via "procedural:type" syntax (e.g., "procedural:cube").
 * 
 * During migration: maintains both legacy Scene and new SceneNew for ECS components.
 */
class SceneManager {
public:
    SceneManager() = default;

    /** Set managers (for resolving material/mesh/texture refs). Call before LoadLevelFromFile or LoadDefaultLevelOrCreate. */
    void SetDependencies(MaterialManager* pMaterialManager_ic, MeshManager* pMeshManager_ic, TextureManager* pTextureManager_ic);

    /** Unload current scene (drops refs; managers can TrimUnused). */
    void UnloadScene();

    /** Current legacy scene (may be null). */
    Scene* GetCurrentScene() { return m_currentScene.get(); }
    const Scene* GetCurrentScene() const { return m_currentScene.get(); }

    /** Current ECS scene (may be null). Contains lights and ECS components. */
    SceneNew* GetSceneNew() { return m_sceneNew.get(); }
    const SceneNew* GetSceneNew() const { return m_sceneNew.get(); }

    /** Set current scene. Replaces any current scene. */
    void SetCurrentScene(std::unique_ptr<Scene> scene);

    /** Load level from JSON (instances[] with source glTF path or "procedural:type" + transform). Returns true on success. */
    bool LoadLevelFromFile(const std::string& path);

    /** Ensure default level file and primitives exist; create if missing. Never overwrites existing files. */
    void EnsureDefaultLevelFile(const std::string& path);

    /** Ensure default level file exists, then load it. Use at startup. Returns true on success. */
    bool LoadDefaultLevelOrCreate(const std::string& defaultLevelPath);

    /** Add object to current scene. No-op if no current scene. */
    void AddObject(Object obj);

    /** Remove object at index. No-op if index out of range. */
    void RemoveObject(size_t index);

    /**
     * Sync transforms from SceneNew GameObjects back to Scene Objects.
     * Call each frame before rendering to ensure editor changes to mesh transforms are reflected.
     */
    void SyncTransformsToScene();

private:
    /** Helper: check if source is procedural (starts with "procedural:"), extract type, return mesh. */
    std::shared_ptr<MeshHandle> LoadProceduralMesh(const std::string& source);

    /** Load lights from JSON into SceneNew. */
    void LoadLightsFromJson(const nlohmann::json& j);

    /**
     * Future-work hooks: if called, log once per glTF so animation/skinning tasks are not forgotten.
     */
    void PrepareAnimationImportStub(const tinygltf::Model& model, const std::string& gltfPath);
    void PrepareSkinningImportStub(const tinygltf::Model& model, const tinygltf::Primitive& prim, const std::string& gltfPath);

    GltfLoader m_gltfLoader;
    MaterialManager* m_pMaterialManager = nullptr;
    MeshManager*     m_pMeshManager     = nullptr;
    TextureManager*  m_pTextureManager  = nullptr;
    std::unique_ptr<Scene> m_currentScene;
    std::unique_ptr<SceneNew> m_sceneNew;
    
    /** Cache for procedural meshes (type -> mesh). */
    std::map<std::string, std::shared_ptr<MeshHandle>> m_proceduralMeshCache;
};
