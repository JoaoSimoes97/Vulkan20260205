#pragma once

#include "gltf_loader.h"
#include "scene/scene.h"
#include <memory>
#include <string>
#include <map>

class MaterialManager;
class MeshManager;
class TextureManager;

/**
 * SceneManager: owns current scene and level loading. Level = JSON descriptor + many glTFs (one per instance).
 * SetDependencies() must be called before LoadLevelFromFile or LoadDefaultLevelOrCreate.
 * Supports procedural meshes via "procedural:type" syntax (e.g., "procedural:cube").
 */
class SceneManager {
public:
    SceneManager() = default;

    /** Set managers (for resolving material/mesh/texture refs). Call before LoadLevelFromFile or LoadDefaultLevelOrCreate. */
    void SetDependencies(MaterialManager* pMaterialManager_ic, MeshManager* pMeshManager_ic, TextureManager* pTextureManager_ic);

    /** Unload current scene (drops refs; managers can TrimUnused). */
    void UnloadScene();

    /** Current scene (may be null). */
    Scene* GetCurrentScene() { return m_currentScene.get(); }
    const Scene* GetCurrentScene() const { return m_currentScene.get(); }

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

private:
    /** Helper: check if source is procedural (starts with "procedural:"), extract type, return mesh. */
    std::shared_ptr<MeshHandle> LoadProceduralMesh(const std::string& source);

    GltfLoader m_gltfLoader;
    MaterialManager* m_pMaterialManager = nullptr;
    MeshManager*     m_pMeshManager     = nullptr;
    TextureManager*  m_pTextureManager  = nullptr;
    std::unique_ptr<Scene> m_currentScene;
    
    /** Cache for procedural meshes (type -> mesh). */
    std::map<std::string, std::shared_ptr<MeshHandle>> m_proceduralMeshCache;
};
