#pragma once

#include "gltf_loader.h"
#include "scene/scene.h"
#include <memory>
#include <string>

class MaterialManager;
class MeshManager;

/**
 * SceneManager: owns current scene and level loading. Level = JSON descriptor + many glTFs (one per instance).
 * SetDependencies() must be called before LoadLevelFromFile or LoadDefaultLevelOrCreate.
 */
class SceneManager {
public:
    SceneManager() = default;

    /** Set managers (for resolving material/mesh refs). Call before LoadLevelFromFile or LoadDefaultLevelOrCreate. */
    void SetDependencies(MaterialManager* pMaterialManager_ic, MeshManager* pMeshManager_ic);

    /** Unload current scene (drops refs; managers can TrimUnused). */
    void UnloadScene();

    /** Current scene (may be null). */
    Scene* GetCurrentScene() { return m_currentScene.get(); }
    const Scene* GetCurrentScene() const { return m_currentScene.get(); }

    /** Set current scene. Replaces any current scene. */
    void SetCurrentScene(std::unique_ptr<Scene> scene);

    /** Load level from JSON (instances[] with source glTF path + transform). Returns true on success. */
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
    GltfLoader m_gltfLoader;
    MaterialManager* m_pMaterialManager = nullptr;
    MeshManager* m_pMeshManager = nullptr;
    std::unique_ptr<Scene> m_currentScene;
};
