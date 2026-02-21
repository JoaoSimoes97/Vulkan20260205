/*
 * SceneNew — Implementation.
 */
#include "scene_new.h"
#include <algorithm>
#include <cstring>

uint32_t SceneNew::CreateGameObject(const std::string& name) {
    uint32_t id = m_nextId++;
    
    GameObject go;
    go.id = id;
    go.name = name;
    go.bActive = true;
    
    // Every GameObject gets a Transform
    go.transformIndex = static_cast<uint32_t>(m_transforms.size());
    m_transforms.emplace_back();
    
    m_idToIndex[id] = m_gameObjects.size();
    m_gameObjects.push_back(std::move(go));
    
    return id;
}

bool SceneNew::DestroyGameObject(uint32_t id) {
    auto it = m_idToIndex.find(id);
    if (it == m_idToIndex.end())
        return false;
    
    size_t index = it->second;
    
    // Mark as inactive rather than removing (preserves indices)
    // Full removal with index compaction is more complex
    m_gameObjects[index].bActive = false;
    m_idToIndex.erase(it);
    
    return true;
}

GameObject* SceneNew::FindGameObject(uint32_t id) {
    auto it = m_idToIndex.find(id);
    if (it == m_idToIndex.end())
        return nullptr;
    return &m_gameObjects[it->second];
}

const GameObject* SceneNew::FindGameObject(uint32_t id) const {
    auto it = m_idToIndex.find(id);
    if (it == m_idToIndex.end())
        return nullptr;
    return &m_gameObjects[it->second];
}

GameObject* SceneNew::FindGameObjectByName(const std::string& name) {
    for (auto& go : m_gameObjects) {
        if (go.bActive && go.name == name)
            return &go;
    }
    return nullptr;
}

const GameObject* SceneNew::FindGameObjectByName(const std::string& name) const {
    for (const auto& go : m_gameObjects) {
        if (go.bActive && go.name == name)
            return &go;
    }
    return nullptr;
}

uint32_t SceneNew::AddRenderer(uint32_t gameObjectId, const RendererComponent& renderer) {
    GameObject* go = FindGameObject(gameObjectId);
    if (go == nullptr)
        return INVALID_COMPONENT_INDEX;
    
    if (go->HasRenderer()) {
        // Replace existing
        m_renderers[go->rendererIndex] = renderer;
        m_renderers[go->rendererIndex].gameObjectIndex = static_cast<uint32_t>(m_idToIndex[gameObjectId]);
        return go->rendererIndex;
    }
    
    uint32_t idx = static_cast<uint32_t>(m_renderers.size());
    go->rendererIndex = idx;
    
    RendererComponent r = renderer;
    r.gameObjectIndex = static_cast<uint32_t>(m_idToIndex[gameObjectId]);
    m_renderers.push_back(std::move(r));
    
    return idx;
}

uint32_t SceneNew::AddLight(uint32_t gameObjectId, const LightComponent& light) {
    GameObject* go = FindGameObject(gameObjectId);
    if (go == nullptr)
        return INVALID_COMPONENT_INDEX;
    
    if (go->HasLight()) {
        // Replace existing
        m_lights[go->lightIndex] = light;
        m_lights[go->lightIndex].gameObjectIndex = static_cast<uint32_t>(m_idToIndex[gameObjectId]);
        return go->lightIndex;
    }
    
    uint32_t idx = static_cast<uint32_t>(m_lights.size());
    go->lightIndex = idx;
    
    LightComponent l = light;
    l.gameObjectIndex = static_cast<uint32_t>(m_idToIndex[gameObjectId]);
    m_lights.push_back(std::move(l));
    
    return idx;
}

void SceneNew::RemoveRenderer(uint32_t gameObjectId) {
    GameObject* go = FindGameObject(gameObjectId);
    if (go == nullptr || !go->HasRenderer())
        return;
    
    // Mark slot as inactive rather than removing (preserves indices)
    m_renderers[go->rendererIndex].bVisible = false;
    go->rendererIndex = INVALID_COMPONENT_INDEX;
}

void SceneNew::RemoveLight(uint32_t gameObjectId) {
    GameObject* go = FindGameObject(gameObjectId);
    if (go == nullptr || !go->HasLight())
        return;
    
    m_lights[go->lightIndex].bActive = false;
    go->lightIndex = INVALID_COMPONENT_INDEX;
}

uint32_t SceneNew::AddCamera(uint32_t gameObjectId, const CameraComponent& camera) {
    GameObject* go = FindGameObject(gameObjectId);
    if (go == nullptr)
        return INVALID_COMPONENT_INDEX;
    
    if (go->HasCamera()) {
        // Replace existing
        m_cameras[go->cameraIndex] = camera;
        m_cameras[go->cameraIndex].gameObjectIndex = static_cast<uint32_t>(m_idToIndex[gameObjectId]);
        return go->cameraIndex;
    }
    
    uint32_t idx = static_cast<uint32_t>(m_cameras.size());
    go->cameraIndex = idx;
    
    CameraComponent c = camera;
    c.gameObjectIndex = static_cast<uint32_t>(m_idToIndex[gameObjectId]);
    m_cameras.push_back(std::move(c));
    
    return idx;
}

void SceneNew::RemoveCamera(uint32_t gameObjectId) {
    GameObject* go = FindGameObject(gameObjectId);
    if (go == nullptr || !go->HasCamera())
        return;
    
    // Mark as not main (effectively inactive for this purpose)
    m_cameras[go->cameraIndex].bIsMain = false;
    go->cameraIndex = INVALID_COMPONENT_INDEX;
}

Transform* SceneNew::GetTransform(uint32_t gameObjectId) {
    GameObject* go = FindGameObject(gameObjectId);
    if (go == nullptr)
        return nullptr;
    return &m_transforms[go->transformIndex];
}

const Transform* SceneNew::GetTransform(uint32_t gameObjectId) const {
    const GameObject* go = FindGameObject(gameObjectId);
    if (go == nullptr)
        return nullptr;
    return &m_transforms[go->transformIndex];
}

void SceneNew::UpdateAllTransforms() {
    for (auto& t : m_transforms) {
        TransformBuildModelMatrix(t);
    }
}

void SceneNew::Clear() {
    m_gameObjects.clear();
    m_idToIndex.clear();
    m_transforms.clear();
    m_renderers.clear();
    m_lights.clear();
    m_cameras.clear();
    m_nextId = 1;
}

void SceneNew::FillPushDataForAllObjects(const float* viewProj) {
    // This is for legacy compatibility with the old RenderListBuilder.
    // In the new system, the RenderListBuilder works directly with component pools.
    // For now, this does nothing - push data is built differently in new architecture.
    (void)viewProj;
}

