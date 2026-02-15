#pragma once

#include "scene/scene.h"
#include "thread/job_queue.h"
#include <memory>
#include <string>

class MaterialManager;
class MeshManager;

/**
 * SceneManager: owns current scene, load/unload (async via JobQueue), and object control.
 * SetDependencies() must be called before LoadSceneAsync or CreateDefaultScene.
 */
class SceneManager {
public:
    SceneManager() = default;

    /** Set job queue and managers (for async load and resolving material/mesh refs). Call before LoadSceneAsync or CreateDefaultScene. */
    void SetDependencies(JobQueue* pJobQueue_ic, MaterialManager* pMaterialManager_ic, MeshManager* pMeshManager_ic);

    /** Load scene from file asynchronously. When the file is ready, main thread parses and sets current scene. */
    void LoadSceneAsync(const std::string& path);

    /** Called by app from ProcessCompletedJobs handler. Dispatches completed file load to pending scene load if path matches. */
    void OnCompletedLoad(LoadJobType eType_ic, const std::string& sPath_ic, std::vector<uint8_t> vecData_in);

    /** Unload current scene (drops refs; managers can TrimUnused). */
    void UnloadScene();

    /** Current scene (may be null). */
    Scene* GetCurrentScene() { return m_currentScene.get(); }
    const Scene* GetCurrentScene() const { return m_currentScene.get(); }

    /** Set current scene (e.g. from CreateDefaultScene). Replaces any current scene. */
    void SetCurrentScene(std::unique_ptr<Scene> scene);

    /** Build default scene (9 objects). Requires SetDependencies. Returns new Scene; caller can SetCurrentScene(result). */
    std::unique_ptr<Scene> CreateDefaultScene();

    /** Add object to current scene. No-op if no current scene. */
    void AddObject(Object obj);

    /** Remove object at index. No-op if index out of range. */
    void RemoveObject(size_t index);

private:
    bool ParseSceneJson(const std::string& path, const uint8_t* pData, size_t size, Scene& outScene);

    JobQueue* m_pJobQueue = nullptr;
    MaterialManager* m_pMaterialManager = nullptr;
    MeshManager* m_pMeshManager = nullptr;
    std::unique_ptr<Scene> m_currentScene;
    std::string m_pendingScenePath;
};
