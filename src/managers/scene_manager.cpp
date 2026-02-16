/*
 * SceneManager — level = JSON + many glTFs; LoadLevelFromFile, EnsureDefaultLevelFile, LoadDefaultLevelOrCreate.
 */
#include "scene_manager.h"
#include "material_manager.h"
#include "mesh_manager.h"
#include "texture_manager.h"
#include "scene/object.h"
#include "gltf_mesh_utils.h"
#include "procedural_mesh_factory.h"
#include "vulkan/vulkan_utils.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <tiny_gltf.h>

using json = nlohmann::json;

namespace {

/**
 * Resolve engine pipeline key from glTF material alphaMode and object renderMode.
 * Material (glTF) = appearance (color, texture). RenderMode = visualization choice (solid, wireframe).
 * No fallbacks: if unresolved, returns empty.
 */
std::string ResolvePipelineKey(const std::string& alphaMode, RenderMode renderMode) {
    // Explicit render mode override
    if (renderMode == RenderMode::Wireframe) return "wire";
    if (renderMode == RenderMode::Solid) return "main";
    
    // Auto: use material alphaMode
    if (alphaMode == "OPAQUE") return "main";
    if (alphaMode == "MASK") return "mask";
    if (alphaMode == "BLEND") return "transparent";
    
    return {};  // Unrecognized alphaMode
}

/**
 * Default level JSON: multiple cubes with different colors and render modes.
 * Material (color) from glTF; renderMode (solid vs wireframe) per instance.
 */
json GetDefaultLevelJson() {
    return {
        { "name", "default" },
        { "instances", json::array({
            { { "source", "primitives/cube_red.glb" },    { "position", json::array({ 0.0, 0.0, 0.0 }) },   { "renderMode", "auto" } },
            { { "source", "primitives/cube_red.glb" },    { "position", json::array({ -2.5, 0.0, 0.0 }) }, { "renderMode", "wireframe" } },
            { { "source", "primitives/cube_yellow.glb" }, { "position", json::array({ 2.5, 0.0, 0.0 }) },  { "renderMode", "auto" } },
            { { "source", "primitives/cube_blue.glb" },   { "position", json::array({ 0.0, 2.5, 0.0 }) },  { "renderMode", "wireframe" } }
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
 * Build a minimal tinygltf::Model: single cube mesh (36 vertices) with one material (OPAQUE).
 * Material = appearance only (color); render mode set separately at runtime.
 */
tinygltf::Model BuildMinimalCubeModel(const std::string& materialName,
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
    mat.pbrMetallicRoughness.baseColorFactor = baseColor;
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
                                const std::vector<double>& baseColor) {
    std::filesystem::path filePath = baseDir / "primitives" / filename;
    std::ifstream in(filePath);
    if (in.is_open()) {
        in.close();
        return;
    }
    std::filesystem::path primDir = baseDir / "primitives";
    std::filesystem::create_directories(primDir);
    tinygltf::Model model = BuildMinimalCubeModel(materialName, baseColor);
    if (loader.WriteToFile(model, filePath.string()))
        VulkanUtils::LogInfo("SceneManager: created default {}", filePath.string());
}

/** Ensure all default primitives exist (cubes with different colors for variety). */
void EnsureDefaultPrimitives(const std::filesystem::path& baseDir, GltfLoader& loader) {
    EnsureDefaultPrimitiveGltf(baseDir, loader, "cube_red.glb",    "Red Cube",    {1.0, 0.2, 0.2, 1.0});
    EnsureDefaultPrimitiveGltf(baseDir, loader, "cube_yellow.glb", "Yellow Cube", {1.0, 1.0, 0.0, 1.0});
    EnsureDefaultPrimitiveGltf(baseDir, loader, "cube_blue.glb",   "Blue Cube",   {0.2, 0.4, 1.0, 1.0});
}

} // namespace

void SceneManager::SetDependencies(MaterialManager* pMaterialManager_ic, MeshManager* pMeshManager_ic, TextureManager* pTextureManager_ic) {
    m_pMaterialManager = pMaterialManager_ic;
    m_pMeshManager = pMeshManager_ic;
    m_pTextureManager = pTextureManager_ic;
}

void SceneManager::UnloadScene() {
    m_currentScene.reset();
    // Clear procedural mesh cache to release all mesh handles
    m_proceduralMeshCache.clear();
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

        // Check if this is a procedural mesh
        std::shared_ptr<MeshHandle> pMesh;
        std::shared_ptr<TextureHandle> pTexture;
        std::shared_ptr<MaterialHandle> pMaterial;
        float baseColorFromSource[4] = {1.f, 1.f, 1.f, 1.f};
        
        if (source.find("procedural:") == 0) {
            // Procedural mesh
            pMesh = LoadProceduralMesh(source);
            if (!pMesh) {
                VulkanUtils::LogErr("SceneManager: failed to create procedural mesh \"{}\"", source);
                continue;
            }
            
            // Use default material (solid opaque)
            RenderMode renderMode = RenderMode::Auto;
            if (jInst.contains("renderMode") && jInst["renderMode"].is_string()) {
                std::string modeStr = jInst["renderMode"].get<std::string>();
                if (modeStr == "solid") renderMode = RenderMode::Solid;
                else if (modeStr == "wireframe") renderMode = RenderMode::Wireframe;
                else if (modeStr == "auto") renderMode = RenderMode::Auto;
            }
            
            std::string pipelineKey = ResolvePipelineKey("OPAQUE", renderMode);
            pMaterial = m_pMaterialManager->GetMaterial(pipelineKey);
            if (!pMaterial) {
                VulkanUtils::LogErr("SceneManager: pipeline \"{}\" not registered for procedural \"{}\"", pipelineKey, source);
                continue;
            }
            
            // Color override from JSON
            if (jInst.contains("color") && jInst["color"].is_array() && jInst["color"].size() >= 4) {
                baseColorFromSource[0] = static_cast<float>(jInst["color"][0].get<double>());
                baseColorFromSource[1] = static_cast<float>(jInst["color"][1].get<double>());
                baseColorFromSource[2] = static_cast<float>(jInst["color"][2].get<double>());
                baseColorFromSource[3] = static_cast<float>(jInst["color"][3].get<double>());
            }
            
            // No texture for procedural meshes (unless we add support later)
            pTexture = nullptr;
            
        } else {
            // glTF mesh
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

            // Render mode: optional override in level JSON; defaults to Auto
            RenderMode renderMode = RenderMode::Auto;
            if (jInst.contains("renderMode") && jInst["renderMode"].is_string()) {
                std::string modeStr = jInst["renderMode"].get<std::string>();
                if (modeStr == "solid") renderMode = RenderMode::Solid;
                else if (modeStr == "wireframe") renderMode = RenderMode::Wireframe;
                else if (modeStr == "auto") renderMode = RenderMode::Auto;
                else {
                    VulkanUtils::LogErr("SceneManager: unknown renderMode \"{}\" for \"{}\"", modeStr, gltfPath);
                    continue;
                }
            }

            std::string pipelineKey = ResolvePipelineKey(gltfMat.alphaMode, renderMode);
            if (pipelineKey.empty()) {
                VulkanUtils::LogErr("SceneManager: glTF \"{}\" material alphaMode \"{}\" with renderMode could not be mapped to a pipeline", gltfPath, gltfMat.alphaMode);
                continue;
            }
            pMaterial = m_pMaterialManager->GetMaterial(pipelineKey);
            if (!pMaterial) {
                VulkanUtils::LogErr("SceneManager: pipeline \"{}\" not registered for glTF \"{}\"", pipelineKey, gltfPath);
                continue;
            }
            
            // Extract mesh
            std::vector<VertexData> vertices;
            std::vector<uint32_t> indices;
            if (!GetMeshDataFromGltf(*model, 0, 0, vertices)) {
                VulkanUtils::LogErr("SceneManager: ExtractVertexData failed for \"{}\"", gltfPath);
                continue;
            }
            const uint32_t vertexCount = static_cast<uint32_t>(vertices.size());
            pMesh = m_pMeshManager->GetOrCreateFromGltf(gltfPath, vertices.data(), vertexCount);
            if (!pMesh) {
                VulkanUtils::LogErr("SceneManager: GetOrCreateFromGltf failed for \"{}\"", gltfPath);
                continue;
            }
            
            // Base color from glTF
            baseColorFromSource[0] = static_cast<float>(baseColor[0]);
            baseColorFromSource[1] = static_cast<float>(baseColor[1]);
            baseColorFromSource[2] = static_cast<float>(baseColor[2]);
            baseColorFromSource[3] = static_cast<float>(baseColor[3]);
            
            // Texture
            if (m_pTextureManager && gltfMat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                int texIdx = gltfMat.pbrMetallicRoughness.baseColorTexture.index;
                if (texIdx >= 0 && size_t(texIdx) < model->textures.size()) {
                    const tinygltf::Texture& tex = model->textures[size_t(texIdx)];
                    if (tex.source >= 0 && size_t(tex.source) < model->images.size()) {
                        const tinygltf::Image& img = model->images[size_t(tex.source)];
                        if (!img.image.empty() && img.width > 0 && img.height > 0 && img.component > 0) {
                            std::string texName = img.uri.empty() ? ("tex_" + gltfPath + "_" + std::to_string(tex.source)) : img.uri;
                            pTexture = m_pTextureManager->GetOrCreateFromMemory(texName, img.width, img.height, img.component, img.image.data());
                        }
                    }
                }
            }
        }

        // Common object creation (both procedural and glTF)
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

        // Create object
        Object obj;
        obj.pMesh = pMesh;
        obj.pMaterial = pMaterial;
        obj.pTexture = pTexture;
        ObjectSetFromPositionRotationScale(obj.localTransform,
            pos[0], pos[1], pos[2],
            rot[0], rot[1], rot[2], rot[3],
            scale[0], scale[1], scale[2]);
        obj.color[0] = baseColorFromSource[0];
        obj.color[1] = baseColorFromSource[1];
        obj.color[2] = baseColorFromSource[2];
        obj.color[3] = baseColorFromSource[3];
        
        // Load emissive property (for future lighting)
        if (jInst.contains("emissive") && jInst["emissive"].is_array() && jInst["emissive"].size() >= 4) {
            obj.emissive[0] = static_cast<float>(jInst["emissive"][0].get<double>());
            obj.emissive[1] = static_cast<float>(jInst["emissive"][1].get<double>());
            obj.emissive[2] = static_cast<float>(jInst["emissive"][2].get<double>());
            obj.emissive[3] = static_cast<float>(jInst["emissive"][3].get<double>());
        }
        
        obj.pushData.resize(kObjectPushConstantSize);
        obj.pushDataSize = kObjectPushConstantSize;
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

std::shared_ptr<MeshHandle> SceneManager::LoadProceduralMesh(const std::string& source) {
    // Check if it's a procedural mesh
    if (source.find("procedural:") != 0)
        return nullptr;
    
    // Extract type
    std::string type = source.substr(11); // Skip "procedural:"
    
    // Check cache
    auto it = m_proceduralMeshCache.find(type);
    if (it != m_proceduralMeshCache.end())
        return it->second;
    
    // Create new procedural mesh
    std::shared_ptr<MeshHandle> pMesh = ProceduralMeshFactory::CreateMesh(type, m_pMeshManager);
    if (pMesh) {
        m_proceduralMeshCache[type] = pMesh;
    }
    
    return pMesh;
}
