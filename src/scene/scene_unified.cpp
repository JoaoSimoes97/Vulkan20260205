/**
 * Scene — Unified ECS scene implementation.
 *
 * Phase 4.2: Unified Scene System
 */

#include "scene_unified.h"
#include <algorithm>
#include <cmath>
#include <cstring>

/* ======== GameObject Management ======== */

uint32_t Scene::CreateGameObject(const std::string& name) {
    uint32_t id = m_nextId++;
    
    GameObject go;
    go.id = id;
    go.name = name.empty() ? ("GameObject_" + std::to_string(id)) : name;
    
    m_idToIndex[id] = m_gameObjects.size();
    m_gameObjects.push_back(std::move(go));
    
    MarkDirty(SceneDirtyFlags::Structure);
    NotifyChange();
    
    return id;
}

bool Scene::DestroyGameObject(uint32_t id) {
    auto it = m_idToIndex.find(id);
    if (it == m_idToIndex.end()) {
        return false;
    }
    
    size_t index = it->second;
    
    // Remove components associated with this GameObject
    m_transformMap.erase(id);
    m_rendererMap.erase(id);
    m_lightMap.erase(id);
    m_cameraMap.erase(id);
    
    // Swap-and-pop for efficient removal
    if (index < m_gameObjects.size() - 1) {
        std::swap(m_gameObjects[index], m_gameObjects.back());
        // Update index map for swapped element
        m_idToIndex[m_gameObjects[index].id] = index;
    }
    
    m_gameObjects.pop_back();
    m_idToIndex.erase(id);
    
    MarkDirty(SceneDirtyFlags::Structure);
    NotifyChange();
    
    return true;
}

GameObject* Scene::FindGameObject(uint32_t id) {
    auto it = m_idToIndex.find(id);
    if (it == m_idToIndex.end()) {
        return nullptr;
    }
    return &m_gameObjects[it->second];
}

const GameObject* Scene::FindGameObject(uint32_t id) const {
    auto it = m_idToIndex.find(id);
    if (it == m_idToIndex.end()) {
        return nullptr;
    }
    return &m_gameObjects[it->second];
}

GameObject* Scene::FindGameObjectByName(const std::string& name) {
    for (auto& go : m_gameObjects) {
        if (go.name == name) {
            return &go;
        }
    }
    return nullptr;
}

const GameObject* Scene::FindGameObjectByName(const std::string& name) const {
    for (const auto& go : m_gameObjects) {
        if (go.name == name) {
            return &go;
        }
    }
    return nullptr;
}

/* ======== Component Add/Remove ======== */

uint32_t Scene::AddTransform(uint32_t gameObjectId, const Transform& transform) {
    if (m_idToIndex.find(gameObjectId) == m_idToIndex.end()) {
        return UINT32_MAX;
    }
    
    uint32_t componentIndex = static_cast<uint32_t>(m_transforms.size());
    m_transforms.push_back(transform);
    m_transformMap[gameObjectId] = componentIndex;
    
    // Update GameObject index
    if (auto* go = FindGameObject(gameObjectId)) {
        go->transformIndex = componentIndex;
    }
    
    MarkDirty(SceneDirtyFlags::Transforms);
    return componentIndex;
}

uint32_t Scene::AddRenderer(uint32_t gameObjectId, const RendererComponent& renderer) {
    if (m_idToIndex.find(gameObjectId) == m_idToIndex.end()) {
        return UINT32_MAX;
    }
    
    uint32_t componentIndex = static_cast<uint32_t>(m_renderers.size());
    m_renderers.push_back(renderer);
    m_rendererMap[gameObjectId] = componentIndex;
    
    // Update GameObject index
    if (auto* go = FindGameObject(gameObjectId)) {
        go->rendererIndex = componentIndex;
    }
    
    MarkDirty(SceneDirtyFlags::Renderers);
    NotifyChange();
    return componentIndex;
}

uint32_t Scene::AddLight(uint32_t gameObjectId, const LightComponent& light) {
    if (m_idToIndex.find(gameObjectId) == m_idToIndex.end()) {
        return UINT32_MAX;
    }
    
    uint32_t componentIndex = static_cast<uint32_t>(m_lights.size());
    m_lights.push_back(light);
    m_lightMap[gameObjectId] = componentIndex;
    
    // Update GameObject index
    if (auto* go = FindGameObject(gameObjectId)) {
        go->lightIndex = componentIndex;
    }
    
    MarkDirty(SceneDirtyFlags::Lights);
    return componentIndex;
}

uint32_t Scene::AddCamera(uint32_t gameObjectId, const CameraComponent& camera) {
    if (m_idToIndex.find(gameObjectId) == m_idToIndex.end()) {
        return UINT32_MAX;
    }
    
    uint32_t componentIndex = static_cast<uint32_t>(m_cameras.size());
    m_cameras.push_back(camera);
    m_cameraMap[gameObjectId] = componentIndex;
    
    // Update GameObject index
    if (auto* go = FindGameObject(gameObjectId)) {
        go->cameraIndex = componentIndex;
    }
    
    MarkDirty(SceneDirtyFlags::Cameras);
    return componentIndex;
}

Transform* Scene::GetTransform(uint32_t gameObjectId) {
    auto it = m_transformMap.find(gameObjectId);
    if (it == m_transformMap.end() || it->second >= m_transforms.size()) {
        return nullptr;
    }
    return &m_transforms[it->second];
}

const Transform* Scene::GetTransform(uint32_t gameObjectId) const {
    auto it = m_transformMap.find(gameObjectId);
    if (it == m_transformMap.end() || it->second >= m_transforms.size()) {
        return nullptr;
    }
    return &m_transforms[it->second];
}

RendererComponent* Scene::GetRenderer(uint32_t gameObjectId) {
    auto it = m_rendererMap.find(gameObjectId);
    if (it == m_rendererMap.end() || it->second >= m_renderers.size()) {
        return nullptr;
    }
    return &m_renderers[it->second];
}

const RendererComponent* Scene::GetRenderer(uint32_t gameObjectId) const {
    auto it = m_rendererMap.find(gameObjectId);
    if (it == m_rendererMap.end() || it->second >= m_renderers.size()) {
        return nullptr;
    }
    return &m_renderers[it->second];
}

LightComponent* Scene::GetLight(uint32_t gameObjectId) {
    auto it = m_lightMap.find(gameObjectId);
    if (it == m_lightMap.end() || it->second >= m_lights.size()) {
        return nullptr;
    }
    return &m_lights[it->second];
}

const LightComponent* Scene::GetLight(uint32_t gameObjectId) const {
    auto it = m_lightMap.find(gameObjectId);
    if (it == m_lightMap.end() || it->second >= m_lights.size()) {
        return nullptr;
    }
    return &m_lights[it->second];
}

CameraComponent* Scene::GetCamera(uint32_t gameObjectId) {
    auto it = m_cameraMap.find(gameObjectId);
    if (it == m_cameraMap.end() || it->second >= m_cameras.size()) {
        return nullptr;
    }
    return &m_cameras[it->second];
}

const CameraComponent* Scene::GetCamera(uint32_t gameObjectId) const {
    auto it = m_cameraMap.find(gameObjectId);
    if (it == m_cameraMap.end() || it->second >= m_cameras.size()) {
        return nullptr;
    }
    return &m_cameras[it->second];
}

/* ======== Transform Hierarchy ======== */

void Scene::UpdateTransformHierarchy() {
    // Update all transform matrices
    // For now, simple: each transform computes its world matrix independently
    // TODO: Implement parent-child hierarchy with proper world matrix propagation
    
    for (auto& transform : m_transforms) {
        TransformBuildModelMatrix(transform);
    }
    
    ClearDirty(SceneDirtyFlags::Transforms);
}

/* ======== Render List Building ======== */

// Helper: extract frustum planes from viewProj matrix
static void ExtractFrustumPlanes(const float* viewProj, float planes[6][4]) {
    // Left plane: row 3 + row 0
    planes[0][0] = viewProj[3]  + viewProj[0];
    planes[0][1] = viewProj[7]  + viewProj[4];
    planes[0][2] = viewProj[11] + viewProj[8];
    planes[0][3] = viewProj[15] + viewProj[12];
    
    // Right plane: row 3 - row 0
    planes[1][0] = viewProj[3]  - viewProj[0];
    planes[1][1] = viewProj[7]  - viewProj[4];
    planes[1][2] = viewProj[11] - viewProj[8];
    planes[1][3] = viewProj[15] - viewProj[12];
    
    // Bottom plane: row 3 + row 1
    planes[2][0] = viewProj[3]  + viewProj[1];
    planes[2][1] = viewProj[7]  + viewProj[5];
    planes[2][2] = viewProj[11] + viewProj[9];
    planes[2][3] = viewProj[15] + viewProj[13];
    
    // Top plane: row 3 - row 1
    planes[3][0] = viewProj[3]  - viewProj[1];
    planes[3][1] = viewProj[7]  - viewProj[5];
    planes[3][2] = viewProj[11] - viewProj[9];
    planes[3][3] = viewProj[15] - viewProj[13];
    
    // Near plane: row 3 + row 2 (Vulkan: depth 0 at near)
    planes[4][0] = viewProj[3]  + viewProj[2];
    planes[4][1] = viewProj[7]  + viewProj[6];
    planes[4][2] = viewProj[11] + viewProj[10];
    planes[4][3] = viewProj[15] + viewProj[14];
    
    // Far plane: row 3 - row 2
    planes[5][0] = viewProj[3]  - viewProj[2];
    planes[5][1] = viewProj[7]  - viewProj[6];
    planes[5][2] = viewProj[11] - viewProj[10];
    planes[5][3] = viewProj[15] - viewProj[14];
    
    // Normalize planes
    for (int i = 0; i < 6; ++i) {
        float len = std::sqrt(planes[i][0]*planes[i][0] + 
                             planes[i][1]*planes[i][1] + 
                             planes[i][2]*planes[i][2]);
        if (len > 0.0001f) {
            planes[i][0] /= len;
            planes[i][1] /= len;
            planes[i][2] /= len;
            planes[i][3] /= len;
        }
    }
}

// Helper: test sphere against frustum
static bool SphereInFrustum(const float planes[6][4], float cx, float cy, float cz, float radius) {
    for (int i = 0; i < 6; ++i) {
        float dist = planes[i][0]*cx + planes[i][1]*cy + planes[i][2]*cz + planes[i][3];
        if (dist < -radius) {
            return false; // Completely outside
        }
    }
    return true;
}

std::vector<RenderObject> Scene::BuildRenderList(const float* viewProj,
                                                   bool frustumCull,
                                                   uint32_t* outCulledCount) const {
    std::vector<RenderObject> result;
    result.reserve(m_renderers.size());
    
    uint32_t culledCount = 0;
    
    // Extract frustum planes if culling is enabled
    float planes[6][4];
    if (frustumCull && viewProj != nullptr) {
        ExtractFrustumPlanes(viewProj, planes);
    }
    
    uint32_t objectIndex = 0;
    
    // Iterate through GameObjects with renderers
    for (const auto& go : m_gameObjects) {
        if (!go.HasRenderer()) {
            continue;
        }
        
        // Get components
        const Transform* pTransform = GetTransform(go.id);
        const RendererComponent* pRenderer = GetRenderer(go.id);
        
        if (pRenderer == nullptr) {
            continue;
        }
        
        // Build RenderObject
        RenderObject ro;
        ro.gameObjectId = go.id;
        ro.pTransform = pTransform;
        ro.pRenderer = pRenderer;
        ro.objectIndex = objectIndex++;
        
        // Copy resource handles from RendererComponent
        ro.mesh = pRenderer->mesh;
        ro.material = pRenderer->material;
        ro.texture = pRenderer->texture;
        
        // Get world matrix
        if (pTransform != nullptr) {
            // Ensure matrix is up to date
            TransformBuildModelMatrix(*const_cast<Transform*>(pTransform));
            std::memcpy(ro.worldMatrix, pTransform->modelMatrix, sizeof(float) * 16);
            
            // Position from matrix (translation column)
            ro.boundsCenterX = ro.worldMatrix[12];
            ro.boundsCenterY = ro.worldMatrix[13];
            ro.boundsCenterZ = ro.worldMatrix[14];
            
            // Estimate radius from scale (max of XYZ scales)
            float scaleX = std::sqrt(ro.worldMatrix[0]*ro.worldMatrix[0] + ro.worldMatrix[1]*ro.worldMatrix[1] + ro.worldMatrix[2]*ro.worldMatrix[2]);
            float scaleY = std::sqrt(ro.worldMatrix[4]*ro.worldMatrix[4] + ro.worldMatrix[5]*ro.worldMatrix[5] + ro.worldMatrix[6]*ro.worldMatrix[6]);
            float scaleZ = std::sqrt(ro.worldMatrix[8]*ro.worldMatrix[8] + ro.worldMatrix[9]*ro.worldMatrix[9] + ro.worldMatrix[10]*ro.worldMatrix[10]);
            float maxScale = std::max({scaleX, scaleY, scaleZ});
            
            // Use mesh AABB if available, else default radius
            ro.boundsRadius = maxScale * 1.0f; // TODO: Use actual mesh bounds
        } else {
            // Identity matrix
            std::memset(ro.worldMatrix, 0, sizeof(ro.worldMatrix));
            ro.worldMatrix[0] = ro.worldMatrix[5] = ro.worldMatrix[10] = ro.worldMatrix[15] = 1.0f;
            ro.boundsRadius = 1.0f;
        }
        
        // Frustum culling
        if (frustumCull && viewProj != nullptr) {
            if (!SphereInFrustum(planes, ro.boundsCenterX, ro.boundsCenterY, ro.boundsCenterZ, ro.boundsRadius)) {
                ++culledCount;
                continue;
            }
        }
        
        result.push_back(std::move(ro));
    }
    
    if (outCulledCount != nullptr) {
        *outCulledCount = culledCount;
    }
    
    return result;
}

uint32_t Scene::GetRenderableCount() const {
    uint32_t count = 0;
    for (const auto& go : m_gameObjects) {
        if (go.HasRenderer()) {
            ++count;
        }
    }
    return count;
}
