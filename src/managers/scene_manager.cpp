/*
 * SceneManager — current scene ownership, LoadSceneAsync (via JobQueue), CreateDefaultScene, Add/RemoveObject.
 */
#include "scene_manager.h"
#include "material_manager.h"
#include "mesh_manager.h"
#include "scene/object.h"
#include "vulkan/vulkan_utils.h"
#include <nlohmann/json.hpp>
#include <cstring>
#include <stdexcept>

using json = nlohmann::json;

void SceneManager::SetDependencies(JobQueue* pJobQueue_ic, MaterialManager* pMaterialManager_ic, MeshManager* pMeshManager_ic) {
    this->m_pJobQueue = pJobQueue_ic;
    this->m_pMaterialManager = pMaterialManager_ic;
    this->m_pMeshManager = pMeshManager_ic;
}

void SceneManager::LoadSceneAsync(const std::string& path) {
    if (m_pJobQueue == nullptr) {
        VulkanUtils::LogErr("SceneManager::LoadSceneAsync: no JobQueue");
        return;
    }
    m_pendingScenePath = path;
    m_pJobQueue->SubmitLoadFile(path);
}

void SceneManager::OnCompletedLoad(LoadJobType eType_ic, const std::string& sPath_ic, std::vector<uint8_t> vecData_in) {
    if ((eType_ic != LoadJobType::LoadFile) || (sPath_ic != this->m_pendingScenePath) || (this->m_pendingScenePath.empty() == true))
        return;
    this->m_pendingScenePath.clear();
    auto pScene = std::make_unique<Scene>();
    if (ParseSceneJson(sPath_ic, vecData_in.data(), vecData_in.size(), *pScene) == false) {
        VulkanUtils::LogErr("SceneManager: failed to parse scene {}", sPath_ic);
        return;
    }
    SetCurrentScene(std::move(pScene));
    VulkanUtils::LogInfo("SceneManager: loaded scene {}", sPath_ic);
}

void SceneManager::UnloadScene() {
    m_currentScene.reset();
}

void SceneManager::SetCurrentScene(std::unique_ptr<Scene> scene) {
    m_currentScene = std::move(scene);
}

namespace {
    Shape ShapeFromMeshKey(const std::string& key) {
        if (key == "triangle") return Shape::Triangle;
        if (key == "circle") return Shape::Circle;
        if (key == "rectangle") return Shape::Rectangle;
        if (key == "cube") return Shape::Cube;
        return Shape::Triangle;
    }
}

std::unique_ptr<Scene> SceneManager::CreateDefaultScene() {
    if (m_pMaterialManager == nullptr || m_pMeshManager == nullptr) {
        VulkanUtils::LogErr("SceneManager::CreateDefaultScene: SetDependencies not called");
        return nullptr;
    }
    auto scene = std::make_unique<Scene>("default");
    std::vector<Object>& objs = scene->GetObjects();

    auto add = [&](Object o, const char* materialKey, const char* meshKey) {
        o.pMaterial = m_pMaterialManager->GetMaterial(materialKey);
        o.pMesh = m_pMeshManager->GetOrCreateProcedural(meshKey);
        objs.push_back(std::move(o));
    };

    { Object o = MakeTriangle(); ObjectSetTranslation(o.localTransform, -2.5f, 1.2f, -0.8f); add(std::move(o), "main", "triangle"); }
    { Object o = MakeTriangle(); ObjectSetTranslation(o.localTransform,  2.5f, 1.2f,  0.4f); o.color[0]=1.f; o.color[1]=0.5f; o.color[2]=0.f; o.color[3]=1.f; add(std::move(o), "wire", "triangle"); }
    { Object o = MakeCircle();   ObjectSetTranslation(o.localTransform, -2.8f, 0.f,  0.6f); add(std::move(o), "main", "circle"); }
    { Object o = MakeCircle();   ObjectSetTranslation(o.localTransform, -0.8f, 2.5f, -0.4f); o.color[0]=0.5f; o.color[1]=1.f; o.color[2]=0.5f; o.color[3]=1.f; add(std::move(o), "wire", "circle"); }
    { Object o = MakeRectangle(); ObjectSetTranslation(o.localTransform, 2.2f, 0.f, -1.f); add(std::move(o), "main", "rectangle"); }
    { Object o = MakeRectangle(); ObjectSetTranslation(o.localTransform, 3.5f, 1.2f,  0.2f); o.color[0]=0.5f; o.color[1]=0.5f; o.color[2]=1.f; o.color[3]=1.f; add(std::move(o), "wire", "rectangle"); }
    { Object o = MakeCube();     ObjectSetTranslation(o.localTransform, 0.f, 1.5f,  0.8f); add(std::move(o), "main", "cube"); }
    { Object o = MakeCube();     ObjectSetTranslation(o.localTransform, 1.2f, -1.2f, -0.6f); o.color[0]=1.f; o.color[1]=0.8f; o.color[2]=0.2f; o.color[3]=1.f; add(std::move(o), "wire", "cube"); }
    { Object o = MakeTriangle(); ObjectSetTranslation(o.localTransform, 0.f, -2.2f,  1.f); o.color[0]=0.8f; o.color[1]=0.2f; o.color[2]=0.8f; o.color[3]=1.f; add(std::move(o), "alt", "triangle"); }

    return scene;
}

void SceneManager::AddObject(Object obj) {
    if (m_currentScene)
        m_currentScene->GetObjects().push_back(std::move(obj));
}

void SceneManager::RemoveObject(size_t index) {
    if (!m_currentScene) return;
    auto& objs = m_currentScene->GetObjects();
    if (index < objs.size())
        objs.erase(objs.begin() + static_cast<std::ptrdiff_t>(index));
}

bool SceneManager::ParseSceneJson(const std::string& path, const uint8_t* pData, size_t size, Scene& outScene) {
    (void)path;
    if (m_pMaterialManager == nullptr || m_pMeshManager == nullptr)
        return false;
    if (pData == nullptr || size == 0)
        return false;
    try {
        json j = json::parse(pData, pData + size);
        if (j.contains("name") && j["name"].is_string())
            outScene.SetName(j["name"].get<std::string>());
        if (!j.contains("objects") || !j["objects"].is_array())
            return true; /* empty scene */
        for (const auto& jObj : j["objects"]) {
            if (!jObj.is_object()) continue;
            std::string meshKey = "triangle";
            std::string materialKey = "main";
            float pos[3] = { 0.f, 0.f, 0.f };
            float color[4] = { 1.f, 1.f, 1.f, 1.f };
            if (jObj.contains("mesh") && jObj["mesh"].is_string())
                meshKey = jObj["mesh"].get<std::string>();
            if (jObj.contains("material") && jObj["material"].is_string())
                materialKey = jObj["material"].get<std::string>();
            if (jObj.contains("position") && jObj["position"].is_array() && jObj["position"].size() >= 3) {
                pos[0] = static_cast<float>(jObj["position"][0].get<double>());
                pos[1] = static_cast<float>(jObj["position"][1].get<double>());
                pos[2] = static_cast<float>(jObj["position"][2].get<double>());
            }
            if (jObj.contains("color") && jObj["color"].is_array() && jObj["color"].size() >= 4) {
                color[0] = static_cast<float>(jObj["color"][0].get<double>());
                color[1] = static_cast<float>(jObj["color"][1].get<double>());
                color[2] = static_cast<float>(jObj["color"][2].get<double>());
                color[3] = static_cast<float>(jObj["color"][3].get<double>());
            }
            Object o;
            o.shape = ShapeFromMeshKey(meshKey);
            ObjectSetTranslation(o.localTransform, pos[0], pos[1], pos[2]);
            std::memcpy(o.color, color, sizeof(color));
            o.pMaterial = m_pMaterialManager->GetMaterial(materialKey);
            o.pMesh = m_pMeshManager->GetOrCreateProcedural(meshKey);
            o.pushData.resize(kObjectPushConstantSize);
            o.pushDataSize = kObjectPushConstantSize;
            outScene.GetObjects().push_back(std::move(o));
        }
        return true;
    } catch (const json::exception&) {
        return false;
    }
}
