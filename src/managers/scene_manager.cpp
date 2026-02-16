/*
 * SceneManager — level = JSON + many glTFs; LoadLevelFromFile, EnsureDefaultLevelFile, LoadDefaultLevelOrCreate.
 */
#include "scene_manager.h"
#include "material_manager.h"
#include "mesh_manager.h"
#include "scene/object.h"
#include "gltf_mesh_utils.h"
#include "vulkan/vulkan_utils.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <tiny_gltf.h>

using json = nlohmann::json;

namespace {

/**
 * Resolve engine pipeline key from glTF material. No fallbacks: if unresolved, returns empty.
 * 1) extras.pipeline (string) if present
 * 2) alphaMode: OPAQUE -> "main", MASK -> "mask", BLEND -> "transparent"
 */
std::string ResolvePipelineKeyFromGltfMaterial(const tinygltf::Material& mat) {
    if (mat.extras.IsObject()) {
        const tinygltf::Value& p = mat.extras.Get("pipeline");
        if (p.IsString())
            return p.Get<std::string>();
    }
    if (mat.alphaMode == "OPAQUE") return "main";
    if (mat.alphaMode == "MASK") return "mask";
    if (mat.alphaMode == "BLEND") return "transparent";
    return {};
}

/** Default level JSON: multiple cubes with different pipelines (main, wire, alt). */
json GetDefaultLevelJson() {
    return {
        { "name", "default" },
        { "instances", json::array({
            { { "source", "primitives/cube.glb" },       { "position", json::array({ 0.0, 0.0, 0.0 }) },   { "rotation", json::array({ 0.0, 0.0, 0.0, 1.0 }) }, { "scale", json::array({ 1.0, 1.0, 1.0 }) } },
            { { "source", "primitives/cube.glb" },       { "position", json::array({ -2.0, 0.0, 0.0 }) }, { "rotation", json::array({ 0.0, 0.0, 0.0, 1.0 }) }, { "scale", json::array({ 1.0, 1.0, 1.0 }) } },
            { { "source", "primitives/cube_wire.glb" }, { "position", json::array({ 2.0, 0.0, 0.0 }) },  { "rotation", json::array({ 0.0, 0.0, 0.0, 1.0 }) }, { "scale", json::array({ 1.0, 1.0, 1.0 }) } },
            { { "source", "primitives/cube_alt.glb" },  { "position", json::array({ 0.0, 2.0, 0.0 }) },  { "rotation", json::array({ 0.0, 0.0, 0.0, 1.0 }) }, { "scale", json::array({ 1.0, 1.0, 1.0 }) } }
        }) }
    };
}

/** Cube as 36 vertices (12 triangles, non-indexed). Matches MeshManager cube layout. */
void MakeCubePositions(std::vector<float>& out) {
    float s = 0.5f;
    out = {
        -s,-s,-s, s,-s,-s, s,s,-s,  -s,-s,-s, s,s,-s, -s,s,-s,
        -s,-s, s, s,s, s, s,-s, s,  -s,-s, s, -s,s, s, s,s, s,
        -s,-s,-s, -s,s,-s, -s,s, s,  -s,-s,-s, -s,s, s, -s,-s, s,
        s,-s,-s, s,-s, s, s,s, s,   s,-s,-s, s,s, s, s,s,-s,
        -s,-s, s, s,-s, s, s,-s,-s,  -s,-s, s, s,-s,-s, -s,-s,-s,
        -s, s, s, s, s,-s, s, s, s,  -s, s, s, -s, s,-s, s, s,-s
    };
}

/**
 * Build a minimal tinygltf::Model: single cube mesh (36 vertices) with one material.
 * pipelineKeyForExtras: if non-empty, set material.extras.pipeline so engine selects that pipeline (e.g. "wire", "alt").
 * baseColor: RGBA for pbrMetallicRoughness.baseColorFactor.
 */
tinygltf::Model BuildMinimalCubeModel(const std::string& materialName,
                                      const std::string& pipelineKeyForExtras,
                                      const std::vector<double>& baseColor) {
    std::vector<float> positions;
    MakeCubePositions(positions);
    const size_t numBytes = positions.size() * sizeof(float);

    tinygltf::Model model;
    tinygltf::Buffer buf;
    buf.data.resize(numBytes);
    std::memcpy(buf.data.data(), positions.data(), numBytes);
    model.buffers.push_back(std::move(buf));

    tinygltf::BufferView bv;
    bv.buffer = 0;
    bv.byteOffset = 0;
    bv.byteLength = static_cast<int>(numBytes);
    bv.byteStride = 12;
    bv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    model.bufferViews.push_back(bv);

    tinygltf::Accessor acc;
    acc.bufferView = 0;
    acc.byteOffset = 0;
    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    acc.count = 36;
    acc.type = TINYGLTF_TYPE_VEC3;
    model.accessors.push_back(acc);

    tinygltf::Material mat;
    mat.name = materialName;
    mat.alphaMode = "OPAQUE";
    mat.pbrMetallicRoughness.baseColorFactor = baseColor.size() >= 4 ? baseColor : std::vector<double>{1.0, 1.0, 1.0, 1.0};
    if (!pipelineKeyForExtras.empty()) {
        tinygltf::Value::Object o;
        o["pipeline"] = tinygltf::Value(pipelineKeyForExtras);
        mat.extras = tinygltf::Value(std::move(o));
    }
    model.materials.push_back(std::move(mat));

    tinygltf::Primitive prim;
    prim.attributes["POSITION"] = 0;
    prim.material = 0;
    prim.mode = TINYGLTF_MODE_TRIANGLES;

    tinygltf::Mesh mesh;
    mesh.primitives.push_back(prim);
    model.meshes.push_back(std::move(mesh));

    return model;
}

/** Ensure a primitive .glb exists under baseDir; create if missing. */
void EnsureDefaultPrimitiveGltf(const std::filesystem::path& baseDir, GltfLoader& loader,
                                const std::string& filename,
                                const std::string& materialName,
                                const std::string& pipelineKeyForExtras,
                                const std::vector<double>& baseColor) {
    std::filesystem::path filePath = baseDir / "primitives" / filename;
    std::ifstream in(filePath);
    if (in.is_open()) {
        in.close();
        return;
    }
    std::filesystem::path primDir = baseDir / "primitives";
    std::filesystem::create_directories(primDir);
    tinygltf::Model model = BuildMinimalCubeModel(materialName, pipelineKeyForExtras, baseColor);
    if (loader.WriteToFile(model, filePath.string()))
        VulkanUtils::LogInfo("SceneManager: created default {}", filePath.string());
}

/** Ensure all default primitives (cube main, cube_wire, cube_alt) exist. */
void EnsureDefaultPrimitives(const std::filesystem::path& baseDir, GltfLoader& loader) {
    EnsureDefaultPrimitiveGltf(baseDir, loader, "cube.glb",       "Default", "",  {1.0, 0.2, 0.2, 1.0});  /* main pipeline (OPAQUE), red */
    EnsureDefaultPrimitiveGltf(baseDir, loader, "cube_wire.glb",  "Wire",    "wire", {1.0, 1.0, 0.0, 1.0}); /* wire pipeline, yellow */
    EnsureDefaultPrimitiveGltf(baseDir, loader, "cube_alt.glb",   "Alt",     "alt",  {0.2, 0.4, 1.0, 1.0}); /* alt pipeline, blue */
}

} // namespace

void SceneManager::SetDependencies(MaterialManager* pMaterialManager_ic, MeshManager* pMeshManager_ic) {
    m_pMaterialManager = pMaterialManager_ic;
    m_pMeshManager = pMeshManager_ic;
}

void SceneManager::UnloadScene() {
    m_currentScene.reset();
}

void SceneManager::SetCurrentScene(std::unique_ptr<Scene> scene) {
    m_currentScene = std::move(scene);
}

void SceneManager::EnsureDefaultLevelFile(const std::string& path) {
    std::ifstream in(path);
    if (in.is_open()) {
        in.close();
        return;
    }
    std::filesystem::path p(path);
    std::filesystem::path baseDir = p.parent_path();
    if (!baseDir.empty())
        std::filesystem::create_directories(baseDir);
    json j = GetDefaultLevelJson();
    std::ofstream out(path);
    if (out.is_open())
        out << j.dump(2);
    VulkanUtils::LogInfo("SceneManager: created default level \"{}\"", path);
    EnsureDefaultPrimitives(baseDir, m_gltfLoader);
}

bool SceneManager::LoadLevelFromFile(const std::string& path) {
    if (m_pMaterialManager == nullptr || m_pMeshManager == nullptr) {
        VulkanUtils::LogErr("SceneManager::LoadLevelFromFile: SetDependencies not called");
        return false;
    }
    std::ifstream in(path);
    if (!in.is_open()) {
        VulkanUtils::LogErr("SceneManager::LoadLevelFromFile: cannot open \"{}\"", path);
        return false;
    }
    json j;
    try {
        in >> j;
    } catch (const json::exception& e) {
        VulkanUtils::LogErr("SceneManager::LoadLevelFromFile: invalid JSON \"{}\"", path);
        return false;
    }
    in.close();

    std::filesystem::path levelPath(path);
    std::filesystem::path baseDir = levelPath.parent_path();
    if (baseDir.empty())
        baseDir = ".";

    std::string sceneName = "default";
    if (j.contains("name") && j["name"].is_string())
        sceneName = j["name"].get<std::string>();

    auto scene = std::make_unique<Scene>(sceneName);
    std::vector<Object>& objs = scene->GetObjects();

    if (!j.contains("instances") || !j["instances"].is_array()) {
        SetCurrentScene(std::move(scene));
        VulkanUtils::LogInfo("SceneManager: loaded level \"{}\" (no instances)", path);
        return true;
    }

    for (const auto& jInst : j["instances"]) {
        if (!jInst.is_object() || !jInst.contains("source") || !jInst["source"].is_string())
            continue;
        std::string source = jInst["source"].get<std::string>();
        if (source.empty()) continue;

        std::filesystem::path resolvedPath = baseDir / source;
        std::string gltfPath = resolvedPath.string();
        if (!m_gltfLoader.LoadFromFile(gltfPath)) {
            VulkanUtils::LogErr("SceneManager: failed to load glTF \"{}\"", gltfPath);
            continue;
        }
        const tinygltf::Model* model = m_gltfLoader.GetModel();
        if (!model || model->meshes.empty()) {
            VulkanUtils::LogErr("SceneManager: glTF has no meshes \"{}\"", gltfPath);
            continue;
        }
        const tinygltf::Mesh& mesh = model->meshes[0];
        if (mesh.primitives.empty()) {
            VulkanUtils::LogErr("SceneManager: glTF mesh 0 has no primitives \"{}\"", gltfPath);
            continue;
        }
        const tinygltf::Primitive& prim = mesh.primitives[0];
        if (prim.material < 0 || size_t(prim.material) >= model->materials.size()) {
            VulkanUtils::LogErr("SceneManager: glTF \"{}\" primitive has no valid material", gltfPath);
            continue;
        }
        const tinygltf::Material& gltfMat = model->materials[size_t(prim.material)];
        const std::vector<double>& baseColor = gltfMat.pbrMetallicRoughness.baseColorFactor;
        if (baseColor.size() < 4) {
            VulkanUtils::LogErr("SceneManager: glTF \"{}\" material has no baseColorFactor", gltfPath);
            continue;
        }
        std::string pipelineKey = ResolvePipelineKeyFromGltfMaterial(gltfMat);
        if (pipelineKey.empty()) {
            VulkanUtils::LogErr("SceneManager: glTF \"{}\" material could not be mapped to a pipeline (alphaMode or extras.pipeline)", gltfPath);
            continue;
        }
        std::shared_ptr<MaterialHandle> pMaterial = m_pMaterialManager->GetMaterial(pipelineKey);
        if (!pMaterial) {
            VulkanUtils::LogErr("SceneManager: pipeline \"{}\" not registered for glTF \"{}\"", pipelineKey, gltfPath);
            continue;
        }

        float pos[3] = { 0.f, 0.f, 0.f };
        float rot[4] = { 0.f, 0.f, 0.f, 1.f };
        float scale[3] = { 1.f, 1.f, 1.f };
        if (jInst.contains("position") && jInst["position"].is_array() && jInst["position"].size() >= 3) {
            pos[0] = static_cast<float>(jInst["position"][0].get<double>());
            pos[1] = static_cast<float>(jInst["position"][1].get<double>());
            pos[2] = static_cast<float>(jInst["position"][2].get<double>());
        }
        if (jInst.contains("rotation") && jInst["rotation"].is_array() && jInst["rotation"].size() >= 4) {
            rot[0] = static_cast<float>(jInst["rotation"][0].get<double>());
            rot[1] = static_cast<float>(jInst["rotation"][1].get<double>());
            rot[2] = static_cast<float>(jInst["rotation"][2].get<double>());
            rot[3] = static_cast<float>(jInst["rotation"][3].get<double>());
        }
        if (jInst.contains("scale") && jInst["scale"].is_array() && jInst["scale"].size() >= 3) {
            scale[0] = static_cast<float>(jInst["scale"][0].get<double>());
            scale[1] = static_cast<float>(jInst["scale"][1].get<double>());
            scale[2] = static_cast<float>(jInst["scale"][2].get<double>());
        }

        std::vector<float> positions;
        if (!GetMeshPositionsFromGltf(*model, 0, 0, positions)) {
            VulkanUtils::LogErr("SceneManager: failed to get positions from \"{}\" mesh 0", gltfPath);
            continue;
        }
        const uint32_t vertexCount = static_cast<uint32_t>(positions.size() / 3);
        if (vertexCount == 0u) continue;

        std::string cacheKey = source + ":0:0";
        std::shared_ptr<MeshHandle> pMesh = m_pMeshManager->GetOrCreateFromPositions(cacheKey, positions.data(), vertexCount);
        if (!pMesh) continue;

        Object obj;
        obj.pMaterial = pMaterial;
        obj.pMesh = pMesh;
        obj.shape = Shape::Cube;
        ObjectSetFromPositionRotationScale(obj.localTransform,
            pos[0], pos[1], pos[2],
            rot[0], rot[1], rot[2], rot[3],
            scale[0], scale[1], scale[2]);
        obj.color[0] = static_cast<float>(baseColor[0]);
        obj.color[1] = static_cast<float>(baseColor[1]);
        obj.color[2] = static_cast<float>(baseColor[2]);
        obj.color[3] = static_cast<float>(baseColor[3]);
        obj.pushData.resize(kObjectPushConstantSize);
        obj.pushDataSize = kObjectPushConstantSize;
        obj.vertexCount = pMesh->GetVertexCount();
        obj.instanceCount = 1u;
        obj.firstVertex = 0u;
        obj.firstInstance = 0u;
        objs.push_back(std::move(obj));
    }

    const size_t objectCount = objs.size();
    SetCurrentScene(std::move(scene));
    VulkanUtils::LogInfo("SceneManager: loaded level \"{}\" ({} objects)", path, objectCount);
    return true;
}

bool SceneManager::LoadDefaultLevelOrCreate(const std::string& defaultLevelPath) {
    EnsureDefaultLevelFile(defaultLevelPath);
    if (!LoadLevelFromFile(defaultLevelPath)) {
        SetCurrentScene(std::make_unique<Scene>("empty"));
        return false;
    }
    return true;
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
