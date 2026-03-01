#pragma once

#include "gltf_loader.h"
#include "scene/scene_unified.h"
#include "scene/stress_test_generator.h"
#include "scene/object.h"
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
struct GltfNodeVisitorContext;

/**
 * SceneManager: owns current scene and level loading. Level = JSON descriptor + many glTFs (one per instance).
 * SetDependencies() must be called before LoadLevelFromFile or LoadDefaultLevelOrCreate.
 * Uses unified Scene (scene_unified.h) only: GameObjects + component pools; BuildRenderList() for rendering.
 */
class SceneManager {
public:
    SceneManager() = default;

    void SetDependencies(MaterialManager* pMaterialManager_ic, MeshManager* pMeshManager_ic, TextureManager* pTextureManager_ic);

    void UnloadScene();

    /** Current scene (unified: GameObjects, transforms, renderers, lights). May be null. */
    Scene* GetCurrentScene() { return m_currentScene.get(); }
    const Scene* GetCurrentScene() const { return m_currentScene.get(); }

    void SetCurrentScene(std::unique_ptr<Scene> scene);

    bool LoadLevelFromFile(const std::string& path);

    void EnsureDefaultLevelFile(const std::string& path);

    bool LoadDefaultLevelOrCreate(const std::string& defaultLevelPath);

    /** Add object to current scene (converts Object to GameObject + components). No-op if no current scene. */
    void AddObject(Object obj);

    /** Remove object at index. No-op if index out of range. */
    void RemoveObject(size_t index);

    uint32_t GenerateStressTestScene(const StressTestParams& params, const std::string& modelPath);

private:
    std::shared_ptr<MeshHandle> LoadProceduralMesh(const std::string& source);

    void LoadLightsFromJson(const nlohmann::json& j);

    /** Animation/skinning not in alpha: stub only; logs if glTF has animation/skin data. */
    void PrepareAnimationImportStub(const tinygltf::Model& model, const std::string& gltfPath);
    void PrepareSkinningImportStub(const tinygltf::Model& model, const tinygltf::Primitive& prim, const std::string& gltfPath);

    void VisitGltfNode(GltfNodeVisitorContext& ctx, int nodeIndex, const float* parentMatrix);

    const tinygltf::Model* GetOrLoadGltfModel(const std::string& path);

    void ClearGltfCache();

    /** Convert Object to Transform for AddTransform. */
    static void ObjectToTransform(const Object& obj, Transform& out);

    /** Convert Object to RendererComponent for AddRenderer. */
    void ObjectToRenderer(const Object& obj, RendererComponent& out);

    GltfLoader m_gltfLoader;
    MaterialManager* m_pMaterialManager = nullptr;
    MeshManager*     m_pMeshManager     = nullptr;
    TextureManager*  m_pTextureManager  = nullptr;
    std::unique_ptr<Scene> m_currentScene;

    std::map<std::string, std::shared_ptr<MeshHandle>> m_proceduralMeshCache;
};
