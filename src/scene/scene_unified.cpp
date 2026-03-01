/**
 * Scene — Unified ECS scene implementation.
 *
 * Phase 4.2: Unified Scene System
 */

#include "scene_unified.h"
#include "object.h"
#include "core/transform.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

/* ======== Clear & AddObject (compatibility) ======== */

void Scene::Clear() {
    m_gameObjects.clear();
    m_idToIndex.clear();
    m_nextId = 1;
    m_transforms.clear();
    m_renderers.clear();
    m_lights.clear();
    m_cameras.clear();
    m_transformMap.clear();
    m_rendererMap.clear();
    m_lightMap.clear();
    m_cameraMap.clear();
    m_dirtyFlags = SceneDirtyFlags::None;
    NotifyChange();
}

void Scene::AddObject(const Object& obj) {
    uint32_t goId = CreateGameObject(obj.name.empty() ? "Object" : obj.name);
    Transform t;
    TransformFromMatrix(obj.localTransform, t);
    AddTransform(goId, t);
    RendererComponent r;
    r.mesh = obj.pMesh;
    r.material = obj.pMaterial;
    r.texture = obj.pTexture;
    r.pMetallicRoughnessTexture = obj.pMetallicRoughnessTexture;
    r.pEmissiveTexture = obj.pEmissiveTexture;
    r.pNormalTexture = obj.pNormalTexture;
    r.pOcclusionTexture = obj.pOcclusionTexture;
    r.matProps.baseColor[0] = obj.color[0];
    r.matProps.baseColor[1] = obj.color[1];
    r.matProps.baseColor[2] = obj.color[2];
    r.matProps.baseColor[3] = obj.color[3];
    r.matProps.emissive[0] = obj.emissive[0];
    r.matProps.emissive[1] = obj.emissive[1];
    r.matProps.emissive[2] = obj.emissive[2];
    r.matProps.emissive[3] = obj.emissive[3];
    r.matProps.metallic = obj.metallicFactor;
    r.matProps.roughness = obj.roughnessFactor;
    r.bVisible = true;
    r.emitsLight = obj.emitsLight;
    r.emissiveLightRadius = obj.emissiveLightRadius;
    r.emissiveLightIntensity = obj.emissiveLightIntensity;
    r.instanceTier = static_cast<uint8_t>(static_cast<std::underlying_type_t<InstanceTier>>(obj.instanceTier));
    AddRenderer(goId, r);
}

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

namespace {
void ComputeWorldMatrixForObject(Scene* pScene, uint32_t gameObjectId) {
    Transform* pTransform = pScene->GetTransform(gameObjectId);
    if (!pTransform) return;
    TransformBuildModelMatrix(*pTransform);
    if (pTransform->parentId == NO_PARENT) {
        std::memcpy(pTransform->worldMatrix, pTransform->modelMatrix, sizeof(pTransform->worldMatrix));
    } else {
        ComputeWorldMatrixForObject(pScene, pTransform->parentId);
        const Transform* pParent = pScene->GetTransform(pTransform->parentId);
        if (pParent)
            TransformMultiplyMatrices(pParent->worldMatrix, pTransform->modelMatrix, pTransform->worldMatrix);
        else
            std::memcpy(pTransform->worldMatrix, pTransform->modelMatrix, sizeof(pTransform->worldMatrix));
    }
}
}

void Scene::UpdateTransformHierarchy() {
    for (auto& transform : m_transforms) {
        TransformBuildModelMatrix(transform);
    }
    std::vector<uint32_t> roots = GetRootObjects();
    std::function<void(uint32_t, const float*)> updateRecursive =
        [this, &updateRecursive](uint32_t goId, const float* parentWorld) {
            Transform* pTransform = GetTransform(goId);
            if (!pTransform) return;
            TransformComputeWorldMatrix(*pTransform, parentWorld);
            const GameObject* pGO = FindGameObject(goId);
            if (pGO) {
                for (uint32_t childId : pGO->children) {
                    updateRecursive(childId, pTransform->worldMatrix);
                }
            }
        };
    for (uint32_t rootId : roots) {
        updateRecursive(rootId, nullptr);
    }
    ClearDirty(SceneDirtyFlags::Transforms);
}

bool Scene::SetParent(uint32_t childId, uint32_t parentId, bool preserveWorldPosition) {
    if (childId == parentId) return false;
    GameObject* pChild = FindGameObject(childId);
    if (!pChild) return false;
    Transform* pChildTransform = GetTransform(childId);
    if (!pChildTransform) return false;
    if (parentId != NO_PARENT) {
        if (!FindGameObject(parentId) || WouldCreateCycle(childId, parentId)) return false;
    }
    float savedWorldMatrix[16];
    float parentWorldInverse[16];
    std::memcpy(parentWorldInverse, glm::value_ptr(glm::mat4(1.0f)), sizeof(parentWorldInverse));
    if (preserveWorldPosition) {
        ComputeWorldMatrixForObject(this, childId);
        std::memcpy(savedWorldMatrix, pChildTransform->worldMatrix, sizeof(savedWorldMatrix));
        if (parentId != NO_PARENT) {
            ComputeWorldMatrixForObject(this, parentId);
            const Transform* pParentTransform = GetTransform(parentId);
            if (pParentTransform) {
                glm::mat4 parentMat = glm::make_mat4(pParentTransform->worldMatrix);
                std::memcpy(parentWorldInverse, glm::value_ptr(glm::inverse(parentMat)), sizeof(parentWorldInverse));
            }
        }
    }
    uint32_t oldParentId = pChildTransform->parentId;
    if (oldParentId != NO_PARENT) {
        GameObject* pOldParent = FindGameObject(oldParentId);
        if (pOldParent) {
            auto& siblings = pOldParent->children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), childId), siblings.end());
        }
    }
    pChildTransform->parentId = parentId;
    if (parentId != NO_PARENT) {
        GameObject* pNewParent = FindGameObject(parentId);
        if (pNewParent) pNewParent->children.push_back(childId);
    }
    if (preserveWorldPosition) {
        float newLocalMatrix[16];
        if (parentId != NO_PARENT)
            TransformMultiplyMatrices(parentWorldInverse, savedWorldMatrix, newLocalMatrix);
        else
            std::memcpy(newLocalMatrix, savedWorldMatrix, sizeof(newLocalMatrix));
        TransformFromMatrix(newLocalMatrix, *pChildTransform);
        std::memcpy(pChildTransform->modelMatrix, newLocalMatrix, sizeof(pChildTransform->modelMatrix));
        std::memcpy(pChildTransform->worldMatrix, savedWorldMatrix, sizeof(pChildTransform->worldMatrix));
        /* Keep bDirty true so next UpdateTransformHierarchy rebuilds modelMatrix from pos/rot/scale;
           otherwise modelMatrix stays stale and hierarchy recompute uses wrong local matrix. */
        pChildTransform->bDirty = true;
    } else {
        pChildTransform->bDirty = true;
    }
    MarkDirty(SceneDirtyFlags::Transforms);
    return true;
}

uint32_t Scene::GetParent(uint32_t gameObjectId) const {
    const Transform* p = GetTransform(gameObjectId);
    return p ? p->parentId : NO_PARENT;
}

std::vector<uint32_t> Scene::GetRootObjects() const {
    std::vector<uint32_t> roots;
    for (const auto& go : m_gameObjects) {
        const Transform* p = GetTransform(go.id);
        if (p && p->parentId == NO_PARENT) roots.push_back(go.id);
    }
    return roots;
}

const std::vector<uint32_t>* Scene::GetChildren(uint32_t gameObjectId) const {
    const GameObject* pGO = FindGameObject(gameObjectId);
    return pGO ? &pGO->children : nullptr;
}

bool Scene::WouldCreateCycle(uint32_t childId, uint32_t parentId) const {
    uint32_t current = parentId;
    while (current != NO_PARENT) {
        if (current == childId) return true;
        const Transform* p = GetTransform(current);
        if (!p) break;
        current = p->parentId;
    }
    return false;
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
        
        // Copy resource handles and PBR data from RendererComponent
        ro.mesh = pRenderer->mesh;
        ro.material = pRenderer->material;
        ro.texture = pRenderer->texture;
        ro.pMetallicRoughnessTexture = pRenderer->pMetallicRoughnessTexture;
        ro.pEmissiveTexture = pRenderer->pEmissiveTexture;
        ro.pNormalTexture = pRenderer->pNormalTexture;
        ro.pOcclusionTexture = pRenderer->pOcclusionTexture;
        ro.color[0] = pRenderer->matProps.baseColor[0];
        ro.color[1] = pRenderer->matProps.baseColor[1];
        ro.color[2] = pRenderer->matProps.baseColor[2];
        ro.color[3] = pRenderer->matProps.baseColor[3];
        ro.emissive[0] = pRenderer->matProps.emissive[0];
        ro.emissive[1] = pRenderer->matProps.emissive[1];
        ro.emissive[2] = pRenderer->matProps.emissive[2];
        ro.emissive[3] = pRenderer->matProps.emissive[3];
        ro.instanceTier = pRenderer->instanceTier;
        
        // Get world matrix (use worldMatrix after hierarchy update)
        if (pTransform != nullptr) {
            TransformBuildModelMatrix(*const_cast<Transform*>(pTransform));
            std::memcpy(ro.worldMatrix, pTransform->worldMatrix, sizeof(float) * 16);
            
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
