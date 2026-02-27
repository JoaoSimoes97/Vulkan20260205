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
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_set>
#include <tiny_gltf.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

using json = nlohmann::json;

namespace {

/**
 * Resolve engine pipeline key from glTF material properties and object renderMode.
 * Material (glTF) = appearance (color, texture, doubleSided). RenderMode = visualization choice (solid, wireframe).
 * doubleSided materials get "_ds" suffix for no-cull pipeline variant.
 * No fallbacks: if unresolved, returns empty.
 */
std::string ResolvePipelineKey(const std::string& alphaMode, RenderMode renderMode, bool hasTexture, bool doubleSided) {
    std::string key;
    
    // Explicit render mode override (wireframe ignores doubleSided since it's for debugging)
    if (renderMode == RenderMode::Wireframe) return hasTexture ? "wire_tex" : "wire_untex";
    if (renderMode == RenderMode::Solid) key = hasTexture ? "main_tex" : "main_untex";
    else {
        // Auto: use material alphaMode
        if (alphaMode == "OPAQUE") key = hasTexture ? "main_tex" : "main_untex";
        else if (alphaMode == "MASK") key = hasTexture ? "mask_tex" : "mask_untex";
        else if (alphaMode == "BLEND") key = hasTexture ? "transparent_tex" : "transparent_untex";
    }
    
    if (key.empty()) return {};  // Unrecognized alphaMode
    
    // Append double-sided suffix if needed
    if (doubleSided) key += "_ds";
    
    return key;
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

void MatIdentity(float* out16) {
    for (int i = 0; i < 16; ++i)
        out16[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

void MatMultiply(float* out16, const float* a16, const float* b16) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float v = 0.0f;
            for (int k = 0; k < 4; ++k)
                v += a16[row + k * 4] * b16[k + col * 4];
            out16[row + col * 4] = v;
        }
    }
}

void BuildNodeLocalMatrix(const tinygltf::Node& node, float* out16) {
    if (node.matrix.size() == 16u) {
        for (size_t i = 0; i < 16u; ++i)
            out16[i] = static_cast<float>(node.matrix[i]);
        return;
    }

    float tx = 0.f, ty = 0.f, tz = 0.f;
    float qx = 0.f, qy = 0.f, qz = 0.f, qw = 1.f;
    float sx = 1.f, sy = 1.f, sz = 1.f;

    if (node.translation.size() == 3u) {
        tx = static_cast<float>(node.translation[0]);
        ty = static_cast<float>(node.translation[1]);
        tz = static_cast<float>(node.translation[2]);
    }
    if (node.rotation.size() == 4u) {
        qx = static_cast<float>(node.rotation[0]);
        qy = static_cast<float>(node.rotation[1]);
        qz = static_cast<float>(node.rotation[2]);
        qw = static_cast<float>(node.rotation[3]);
    }
    if (node.scale.size() == 3u) {
        sx = static_cast<float>(node.scale[0]);
        sy = static_cast<float>(node.scale[1]);
        sz = static_cast<float>(node.scale[2]);
    }

    ObjectSetFromPositionRotationScale(out16, tx, ty, tz, qx, qy, qz, qw, sx, sy, sz);
}

} // namespace

/**
 * Context struct for glTF node visitor (replaces lambda capture per coding guidelines).
 * Holds all state needed to recursively visit glTF nodes and build Objects.
 */
struct GltfNodeVisitorContext {
    const tinygltf::Model* model;
    const std::string& gltfPath;
    RenderMode renderMode;
    std::vector<Object>& objs;
    const float* instanceTransform;
    bool hasColorOverride;
    const float* colorOverride;
    bool hasEmissiveOverride;
    const float* emissiveOverride;
    bool hasMetallicOverride;
    float metallicOverride;
    bool hasRoughnessOverride;
    float roughnessOverride;
    InstanceTier instanceTier;
    
    // Hierarchy tracking for glTF nodes
    // Maps glTF node index -> first Object index created for that node
    std::unordered_map<int, size_t> nodeToFirstObjIndex;
    // Records (childObjIndex, parentNodeIndex) pairs for hierarchy building
    std::vector<std::pair<size_t, int>> objParentNodePairs;
    // Current parent node index being visited (-1 for root)
    int currentParentNode = -1;
};

void SceneManager::PrepareAnimationImportStub(const tinygltf::Model& model, const std::string& gltfPath) {
    if (model.animations.empty())
        return;
    static std::unordered_set<std::string> warned;
    const std::string key = gltfPath + "#animations";
    if (warned.insert(key).second) {
        VulkanUtils::LogWarn("SceneManager: TODO animation import not implemented yet for \"{}\" ({} clips)",
                             gltfPath, model.animations.size());
    }
}

void SceneManager::PrepareSkinningImportStub(const tinygltf::Model& model, const tinygltf::Primitive& prim, const std::string& gltfPath) {
    const bool hasJoints = prim.attributes.find("JOINTS_0") != prim.attributes.end();
    const bool hasWeights = prim.attributes.find("WEIGHTS_0") != prim.attributes.end();
    const bool needsSkinning = !model.skins.empty() || hasJoints || hasWeights;
    if (!needsSkinning)
        return;

    static std::unordered_set<std::string> warned;
    const std::string key = gltfPath + "#skinning";
    if (warned.insert(key).second) {
        VulkanUtils::LogWarn("SceneManager: TODO skinning import not implemented yet for \"{}\" (skins={}, JOINTS_0={}, WEIGHTS_0={})",
                             gltfPath, model.skins.size(), hasJoints, hasWeights);
    }
}

void SceneManager::VisitGltfNode(GltfNodeVisitorContext& ctx, int nodeIndex, const float* parentMatrix) {
    if (nodeIndex < 0 || size_t(nodeIndex) >= ctx.model->nodes.size())
        return;
    const tinygltf::Node& node = ctx.model->nodes[size_t(nodeIndex)];

    float nodeLocal[16];
    float nodeWorld[16];
    BuildNodeLocalMatrix(node, nodeLocal);
    MatMultiply(nodeWorld, parentMatrix, nodeLocal);
    
    // Track the first Object index we create for this node (for hierarchy mapping)
    bool firstObjForNode = true;

    if (node.mesh >= 0 && size_t(node.mesh) < ctx.model->meshes.size()) {
        const int meshIndex = node.mesh;
        const tinygltf::Mesh& mesh = ctx.model->meshes[size_t(meshIndex)];
        for (size_t primIndex = 0; primIndex < mesh.primitives.size(); ++primIndex) {
            const tinygltf::Primitive& prim = mesh.primitives[primIndex];
            PrepareSkinningImportStub(*ctx.model, prim, ctx.gltfPath);

            if (prim.material < 0 || size_t(prim.material) >= ctx.model->materials.size()) {
                VulkanUtils::LogErr("SceneManager: glTF \"{}\" mesh {} primitive {} has no valid material",
                                   ctx.gltfPath, meshIndex, primIndex);
                continue;
            }

            const tinygltf::Material& gltfMat = ctx.model->materials[size_t(prim.material)];
            const bool hasTexture = (gltfMat.pbrMetallicRoughness.baseColorTexture.index >= 0);
            const bool doubleSided = gltfMat.doubleSided;
            const std::string pipelineKey = ResolvePipelineKey(gltfMat.alphaMode, ctx.renderMode, hasTexture, doubleSided);
            if (pipelineKey.empty()) {
                VulkanUtils::LogErr("SceneManager: glTF \"{}\" mesh {} primitive {} alphaMode \"{}\" could not be mapped",
                                   ctx.gltfPath, meshIndex, primIndex, gltfMat.alphaMode);
                continue;
            }
            std::shared_ptr<MaterialHandle> pMaterial = m_pMaterialManager->GetMaterial(pipelineKey);
            if (!pMaterial) {
                VulkanUtils::LogErr("SceneManager: pipeline \"{}\" not registered for glTF \"{}\"", pipelineKey, ctx.gltfPath);
                continue;
            }

            std::vector<VertexData> vertices;
            if (!GetMeshDataFromGltf(*ctx.model, meshIndex, static_cast<int>(primIndex), vertices)) {
                VulkanUtils::LogErr("SceneManager: ExtractVertexData failed for \"{}\" mesh {} primitive {}",
                                   ctx.gltfPath, meshIndex, primIndex);
                continue;
            }
            const uint32_t vertexCount = static_cast<uint32_t>(vertices.size());
            const std::string meshKey = ctx.gltfPath + ":" + std::to_string(meshIndex) + ":" + std::to_string(primIndex);
            std::shared_ptr<MeshHandle> pMesh = m_pMeshManager->GetOrCreateFromGltf(meshKey, vertices.data(), vertexCount);
            if (!pMesh) {
                VulkanUtils::LogErr("SceneManager: GetOrCreateFromGltf failed for \"{}\" mesh {} primitive {}",
                                   ctx.gltfPath, meshIndex, primIndex);
                continue;
            }

            std::shared_ptr<TextureHandle> pTexture;
            if (m_pTextureManager && hasTexture) {
                int texIdx = gltfMat.pbrMetallicRoughness.baseColorTexture.index;
                if (texIdx >= 0 && size_t(texIdx) < ctx.model->textures.size()) {
                    const tinygltf::Texture& tex = ctx.model->textures[size_t(texIdx)];
                    if (tex.source >= 0 && size_t(tex.source) < ctx.model->images.size()) {
                        const tinygltf::Image& img = ctx.model->images[size_t(tex.source)];
                        if (!img.image.empty() && img.width > 0 && img.height > 0 && img.component > 0) {
                            std::string texName = img.uri.empty()
                                ? ("tex_" + ctx.gltfPath + "_" + std::to_string(tex.source))
                                : img.uri;
                            pTexture = m_pTextureManager->GetOrCreateFromMemory(texName, img.width, img.height, img.component, img.image.data());
                        }
                    }
                }
            }

            // Load metallicRoughnessTexture from glTF (blue=metallic, green=roughness per glTF spec)
            std::shared_ptr<TextureHandle> pMetallicRoughnessTexture;
            if (m_pTextureManager) {
                int mrTexIdx = gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index;
                if (mrTexIdx >= 0 && size_t(mrTexIdx) < ctx.model->textures.size()) {
                    const tinygltf::Texture& mrTex = ctx.model->textures[size_t(mrTexIdx)];
                    if (mrTex.source >= 0 && size_t(mrTex.source) < ctx.model->images.size()) {
                        const tinygltf::Image& mrImg = ctx.model->images[size_t(mrTex.source)];
                        if (!mrImg.image.empty() && mrImg.width > 0 && mrImg.height > 0 && mrImg.component > 0) {
                            std::string mrTexName = mrImg.uri.empty()
                                ? ("mr_tex_" + ctx.gltfPath + "_" + std::to_string(mrTex.source))
                                : ("mr_" + mrImg.uri);
                            pMetallicRoughnessTexture = m_pTextureManager->GetOrCreateFromMemory(mrTexName, mrImg.width, mrImg.height, mrImg.component, mrImg.image.data());
                        }
                    }
                }
            }

            // Load emissiveTexture from glTF (emissive RGB per glTF spec)
            std::shared_ptr<TextureHandle> pEmissiveTexture;
            if (m_pTextureManager) {
                int emTexIdx = gltfMat.emissiveTexture.index;
                if (emTexIdx >= 0 && size_t(emTexIdx) < ctx.model->textures.size()) {
                    const tinygltf::Texture& emTex = ctx.model->textures[size_t(emTexIdx)];
                    if (emTex.source >= 0 && size_t(emTex.source) < ctx.model->images.size()) {
                        const tinygltf::Image& emImg = ctx.model->images[size_t(emTex.source)];
                        if (!emImg.image.empty() && emImg.width > 0 && emImg.height > 0 && emImg.component > 0) {
                            std::string emTexName = emImg.uri.empty()
                                ? ("em_tex_" + ctx.gltfPath + "_" + std::to_string(emTex.source))
                                : ("em_" + emImg.uri);
                            pEmissiveTexture = m_pTextureManager->GetOrCreateFromMemory(emTexName, emImg.width, emImg.height, emImg.component, emImg.image.data());
                        }
                    }
                }
            }

            // Load normalTexture from glTF (tangent-space normal map)
            std::shared_ptr<TextureHandle> pNormalTexture;
            if (m_pTextureManager) {
                int nrmTexIdx = gltfMat.normalTexture.index;
                if (nrmTexIdx >= 0 && size_t(nrmTexIdx) < ctx.model->textures.size()) {
                    const tinygltf::Texture& nrmTex = ctx.model->textures[size_t(nrmTexIdx)];
                    if (nrmTex.source >= 0 && size_t(nrmTex.source) < ctx.model->images.size()) {
                        const tinygltf::Image& nrmImg = ctx.model->images[size_t(nrmTex.source)];
                        if (!nrmImg.image.empty() && nrmImg.width > 0 && nrmImg.height > 0 && nrmImg.component > 0) {
                            std::string nrmTexName = nrmImg.uri.empty()
                                ? ("nrm_tex_" + ctx.gltfPath + "_" + std::to_string(nrmTex.source))
                                : ("nrm_" + nrmImg.uri);
                            pNormalTexture = m_pTextureManager->GetOrCreateFromMemory(nrmTexName, nrmImg.width, nrmImg.height, nrmImg.component, nrmImg.image.data());
                        }
                    }
                }
            }

            // Load occlusionTexture from glTF (ambient occlusion, red channel per spec)
            std::shared_ptr<TextureHandle> pOcclusionTexture;
            if (m_pTextureManager) {
                int occTexIdx = gltfMat.occlusionTexture.index;
                if (occTexIdx >= 0 && size_t(occTexIdx) < ctx.model->textures.size()) {
                    const tinygltf::Texture& occTex = ctx.model->textures[size_t(occTexIdx)];
                    if (occTex.source >= 0 && size_t(occTex.source) < ctx.model->images.size()) {
                        const tinygltf::Image& occImg = ctx.model->images[size_t(occTex.source)];
                        if (!occImg.image.empty() && occImg.width > 0 && occImg.height > 0 && occImg.component > 0) {
                            std::string occTexName = occImg.uri.empty()
                                ? ("occ_tex_" + ctx.gltfPath + "_" + std::to_string(occTex.source))
                                : ("occ_" + occImg.uri);
                            pOcclusionTexture = m_pTextureManager->GetOrCreateFromMemory(occTexName, occImg.width, occImg.height, occImg.component, occImg.image.data());
                        }
                    }
                }
            }

            Object obj;
            obj.pMesh = std::move(pMesh);
            obj.pMaterial = std::move(pMaterial);
            obj.pTexture = std::move(pTexture);
            obj.pMetallicRoughnessTexture = std::move(pMetallicRoughnessTexture);
            obj.pEmissiveTexture = std::move(pEmissiveTexture);
            obj.pNormalTexture = std::move(pNormalTexture);
            obj.pOcclusionTexture = std::move(pOcclusionTexture);

            // Set name from mesh name or node name
            if (!node.name.empty())
                obj.name = node.name;
            else if (!mesh.name.empty())
                obj.name = mesh.name;
            else
                obj.name = ctx.gltfPath + ":" + std::to_string(meshIndex) + ":" + std::to_string(primIndex);

            float combined[16];
            MatMultiply(combined, ctx.instanceTransform, nodeWorld);
            std::memcpy(obj.localTransform, combined, sizeof(combined));

            const std::vector<double>& baseColor = gltfMat.pbrMetallicRoughness.baseColorFactor;
            if (baseColor.size() >= 4u) {
                obj.color[0] = static_cast<float>(baseColor[0]);
                obj.color[1] = static_cast<float>(baseColor[1]);
                obj.color[2] = static_cast<float>(baseColor[2]);
                obj.color[3] = static_cast<float>(baseColor[3]);
            }
            if (ctx.hasColorOverride) {
                obj.color[0] = ctx.colorOverride[0];
                obj.color[1] = ctx.colorOverride[1];
                obj.color[2] = ctx.colorOverride[2];
                obj.color[3] = ctx.colorOverride[3];
            }

            // Emissive: Per glTF spec, emissive = emissiveFactor * emissiveTexture.
            // Always load emissiveFactor; shader will multiply by emissiveTexture (or white default if none).
            if (gltfMat.emissiveFactor.size() >= 3u) {
                obj.emissive[0] = static_cast<float>(gltfMat.emissiveFactor[0]);
                obj.emissive[1] = static_cast<float>(gltfMat.emissiveFactor[1]);
                obj.emissive[2] = static_cast<float>(gltfMat.emissiveFactor[2]);
                obj.emissive[3] = 1.0f; // Strength multiplier (always 1.0 for spec compliance)
                
                // Auto-enable light emission if emissive color is non-zero
                float emissiveLen = obj.emissive[0] + obj.emissive[1] + obj.emissive[2];
                if (emissiveLen > 0.001f) {
                    obj.emitsLight = true;
                }
            }
            if (ctx.hasEmissiveOverride) {
                obj.emissive[0] = ctx.emissiveOverride[0];
                obj.emissive[1] = ctx.emissiveOverride[1];
                obj.emissive[2] = ctx.emissiveOverride[2];
                obj.emissive[3] = ctx.emissiveOverride[3];
                
                // Auto-enable light emission if emissive override is non-zero
                float emissiveLen = ctx.emissiveOverride[0] + ctx.emissiveOverride[1] + ctx.emissiveOverride[2];
                if (emissiveLen > 0.001f) {
                    obj.emitsLight = true;
                }
            }

            // Load metallic and roughness factors from glTF material
            obj.metallicFactor = static_cast<float>(gltfMat.pbrMetallicRoughness.metallicFactor);
            obj.roughnessFactor = static_cast<float>(gltfMat.pbrMetallicRoughness.roughnessFactor);
            
            // Apply overrides if specified
            if (ctx.hasMetallicOverride)
                obj.metallicFactor = ctx.metallicOverride;
            if (ctx.hasRoughnessOverride)
                obj.roughnessFactor = ctx.roughnessOverride;
            
            // Load normal texture scale from glTF material (default 1.0)
            obj.normalScale = static_cast<float>(gltfMat.normalTexture.scale);
            
            // Load occlusion texture strength from glTF material (default 1.0)
            obj.occlusionStrength = static_cast<float>(gltfMat.occlusionTexture.strength);

            // Apply instance tier from level JSON
            obj.instanceTier = ctx.instanceTier;

            obj.pushData.resize(kObjectPushConstantSize);
            obj.pushDataSize = kObjectPushConstantSize;
            
            // Track hierarchy: record which node this object came from
            size_t objIndex = ctx.objs.size();
            if (firstObjForNode) {
                ctx.nodeToFirstObjIndex[nodeIndex] = objIndex;
                firstObjForNode = false;
            }
            // Record parent relationship (if this node has a parent in the glTF)
            if (ctx.currentParentNode >= 0) {
                ctx.objParentNodePairs.push_back({objIndex, ctx.currentParentNode});
            }
            
            ctx.objs.push_back(std::move(obj));
        }
    }
    
    // Recurse to children with this node as their parent
    int savedParent = ctx.currentParentNode;
    ctx.currentParentNode = nodeIndex;
    for (int child : node.children)
        VisitGltfNode(ctx, child, nodeWorld);
    ctx.currentParentNode = savedParent;
}

void SceneManager::SetDependencies(MaterialManager* pMaterialManager_ic, MeshManager* pMeshManager_ic, TextureManager* pTextureManager_ic) {
    m_pMaterialManager = pMaterialManager_ic;
    m_pMeshManager = pMeshManager_ic;
    m_pTextureManager = pTextureManager_ic;
}

void SceneManager::UnloadScene() {
    m_currentScene.reset();
    m_sceneNew.reset();
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
    } catch (const json::exception&) {
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

    // Track parent relationships for hierarchy (parsed from JSON, resolved after GameObjects created)
    // Key: object index in objs vector, Value: parent object name (string reference)
    std::vector<std::string> instanceParentNames;
    
    // Track glTF internal hierarchy: (childObjIndex, parentObjIndex) pairs
    std::vector<std::pair<size_t, size_t>> gltfHierarchyPairs;

    if (!j.contains("instances") || !j["instances"].is_array()) {
        SetCurrentScene(std::move(scene));
        VulkanUtils::LogInfo("SceneManager: loaded level \"{}\" (no instances)", path);
        return true;
    }

    for (const auto& jInst : j["instances"]) {
        if (!jInst.is_object() || !jInst.contains("source") || !jInst["source"].is_string())
            continue;
        const std::string source = jInst["source"].get<std::string>();
        
        // Parse instance name (override for source, used for parenting references)
        std::string instanceName;
        if (jInst.contains("name") && jInst["name"].is_string()) {
            instanceName = jInst["name"].get<std::string>();
        }
        
        // Parse parent reference (name of another instance)
        std::string parentName;
        if (jInst.contains("parent") && jInst["parent"].is_string()) {
            parentName = jInst["parent"].get<std::string>();
        }
        if (source.empty())
            continue;

        RenderMode renderMode = RenderMode::Auto;
        if (jInst.contains("renderMode") && jInst["renderMode"].is_string()) {
            const std::string modeStr = jInst["renderMode"].get<std::string>();
            if (modeStr == "solid") renderMode = RenderMode::Solid;
            else if (modeStr == "wireframe") renderMode = RenderMode::Wireframe;
            else if (modeStr == "auto") renderMode = RenderMode::Auto;
            else {
                VulkanUtils::LogErr("SceneManager: unknown renderMode \"{}\" for source \"{}\"", modeStr, source);
                continue;
            }
        }

        // Parse instance tier (default: static)
        InstanceTier instanceTier = InstanceTier::Static;
        if (jInst.contains("instanceTier") && jInst["instanceTier"].is_string()) {
            instanceTier = ParseInstanceTier(jInst["instanceTier"].get<std::string>());
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

        float instanceTransform[16];
        ObjectSetFromPositionRotationScale(instanceTransform,
            pos[0], pos[1], pos[2],
            rot[0], rot[1], rot[2], rot[3],
            scale[0], scale[1], scale[2]);

        bool hasColorOverride = false;
        float colorOverride[4] = {1.f, 1.f, 1.f, 1.f};
        if (jInst.contains("color") && jInst["color"].is_array() && jInst["color"].size() >= 4) {
            hasColorOverride = true;
            colorOverride[0] = static_cast<float>(jInst["color"][0].get<double>());
            colorOverride[1] = static_cast<float>(jInst["color"][1].get<double>());
            colorOverride[2] = static_cast<float>(jInst["color"][2].get<double>());
            colorOverride[3] = static_cast<float>(jInst["color"][3].get<double>());
        }

        bool hasEmissiveOverride = false;
        float emissiveOverride[4] = {0.f, 0.f, 0.f, 1.f};
        if (jInst.contains("emissive") && jInst["emissive"].is_array() && jInst["emissive"].size() >= 4) {
            hasEmissiveOverride = true;
            emissiveOverride[0] = static_cast<float>(jInst["emissive"][0].get<double>());
            emissiveOverride[1] = static_cast<float>(jInst["emissive"][1].get<double>());
            emissiveOverride[2] = static_cast<float>(jInst["emissive"][2].get<double>());
            emissiveOverride[3] = static_cast<float>(jInst["emissive"][3].get<double>());
        }

        // Parse metallic factor (default 1.0 for metals, override in JSON for procedural meshes)
        float metallicOverride = 1.0f;
        bool hasMetallicOverride = false;
        if (jInst.contains("metallic") && jInst["metallic"].is_number()) {
            hasMetallicOverride = true;
            metallicOverride = static_cast<float>(jInst["metallic"].get<double>());
        }

        // Parse roughness factor (default 1.0, override in JSON for procedural meshes)
        float roughnessOverride = 1.0f;
        bool hasRoughnessOverride = false;
        if (jInst.contains("roughness") && jInst["roughness"].is_number()) {
            hasRoughnessOverride = true;
            roughnessOverride = static_cast<float>(jInst["roughness"].get<double>());
        }

        if (source.rfind("procedural:", 0) == 0) {
            std::shared_ptr<MeshHandle> pMesh = LoadProceduralMesh(source);
            if (!pMesh) {
                VulkanUtils::LogErr("SceneManager: failed to create procedural mesh \"{}\"", source);
                continue;
            }

            // Use textured pipeline for procedural meshes - default white texture enables PBR with factors
            const std::string pipelineKey = ResolvePipelineKey("OPAQUE", renderMode, true, false);
            std::shared_ptr<MaterialHandle> pMaterial = m_pMaterialManager->GetMaterial(pipelineKey);
            if (!pMaterial) {
                VulkanUtils::LogErr("SceneManager: pipeline \"{}\" not registered for procedural \"{}\"", pipelineKey, source);
                continue;
            }

            Object obj;
            // Use explicit name from JSON if provided, otherwise fall back to source
            obj.name = instanceName.empty() ? source : instanceName;
            obj.pMesh = pMesh;
            obj.pMaterial = pMaterial;
            // Assign all PBR textures with proper defaults for full PBR support
            if (m_pTextureManager) {
                obj.pTexture = m_pTextureManager->GetOrCreateDefaultTexture();                    // White base color
                obj.pMetallicRoughnessTexture = m_pTextureManager->GetOrCreateDefaultMRTexture(); // MR factors used as-is
                obj.pEmissiveTexture = m_pTextureManager->GetOrCreateDefaultEmissiveTexture();    // No emission by default
                obj.pNormalTexture = m_pTextureManager->GetOrCreateDefaultNormalTexture();        // Flat normal
                obj.pOcclusionTexture = m_pTextureManager->GetOrCreateDefaultOcclusionTexture();  // No occlusion
            }
            std::memcpy(obj.localTransform, instanceTransform, sizeof(instanceTransform));
            if (hasColorOverride) {
                obj.color[0] = colorOverride[0];
                obj.color[1] = colorOverride[1];
                obj.color[2] = colorOverride[2];
                obj.color[3] = colorOverride[3];
            }
            if (hasEmissiveOverride) {
                obj.emissive[0] = emissiveOverride[0];
                obj.emissive[1] = emissiveOverride[1];
                obj.emissive[2] = emissiveOverride[2];
                obj.emissive[3] = emissiveOverride[3];
                
                // Auto-enable light emission if emissive override is non-zero
                float emissiveLen = emissiveOverride[0] + emissiveOverride[1] + emissiveOverride[2];
                if (emissiveLen > 0.001f) {
                    obj.emitsLight = true;
                }
            }
            if (hasMetallicOverride) {
                obj.metallicFactor = metallicOverride;
            }
            if (hasRoughnessOverride) {
                obj.roughnessFactor = roughnessOverride;
            }
            obj.instanceTier = instanceTier;
            obj.pushData.resize(kObjectPushConstantSize);
            obj.pushDataSize = kObjectPushConstantSize;
            instanceParentNames.push_back(parentName);  // Track parent for hierarchy resolution
            objs.push_back(std::move(obj));
            continue;
        }

        const std::filesystem::path resolvedPath = baseDir / source;
        const std::string gltfPath = resolvedPath.string();
        if (!m_gltfLoader.LoadFromFile(gltfPath)) {
            VulkanUtils::LogErr("SceneManager: failed to load glTF \"{}\"", gltfPath);
            continue;
        }
        const tinygltf::Model* model = m_gltfLoader.GetModel();
        if (!model || model->meshes.empty()) {
            VulkanUtils::LogErr("SceneManager: glTF has no meshes \"{}\"", gltfPath);
            continue;
        }

        PrepareAnimationImportStub(*model, gltfPath);

        std::vector<int> roots;
        if (!model->scenes.empty()) {
            int sceneIndex = model->defaultScene;
            if (sceneIndex < 0 || size_t(sceneIndex) >= model->scenes.size())
                sceneIndex = 0;
            const tinygltf::Scene& sceneDef = model->scenes[size_t(sceneIndex)];
            roots.assign(sceneDef.nodes.begin(), sceneDef.nodes.end());
        }
        if (roots.empty()) {
            roots.reserve(model->nodes.size());
            for (size_t i = 0; i < model->nodes.size(); ++i)
                roots.push_back(static_cast<int>(i));
        }

        GltfNodeVisitorContext ctx{
            model,
            gltfPath,
            renderMode,
            objs,
            instanceTransform,
            hasColorOverride,
            colorOverride,
            hasEmissiveOverride,
            emissiveOverride,
            hasMetallicOverride,
            metallicOverride,
            hasRoughnessOverride,
            roughnessOverride,
            instanceTier,
            {},  // nodeToFirstObjIndex
            {},  // objParentNodePairs
            -1   // currentParentNode (root)
        };
        
        // Track how many objects exist before loading this glTF
        size_t objCountBefore = objs.size();
        
        float identity[16];
        MatIdentity(identity);
        for (int rootNode : roots)
            VisitGltfNode(ctx, rootNode, identity);
        
        // Apply the same parent reference to all objects loaded from this glTF instance
        // Also track glTF internal hierarchy for later resolution
        size_t objCountAfter = objs.size();
        for (size_t objIdx = objCountBefore; objIdx < objCountAfter; ++objIdx) {
            instanceParentNames.push_back(parentName);
        }
        
        // Store glTF hierarchy info: (childObjIndex, parentObjIndex) pairs
        // Convert from (childObjIndex, parentNodeIndex) to (childObjIndex, parentObjIndex)
        for (const auto& pair : ctx.objParentNodePairs) {
            size_t childObjIdx = pair.first;
            int parentNodeIdx = pair.second;
            auto it = ctx.nodeToFirstObjIndex.find(parentNodeIdx);
            if (it != ctx.nodeToFirstObjIndex.end()) {
                // Store negative index to mark this as glTF internal hierarchy (vs JSON parent name)
                // We'll resolve this after GameObjects are created
                gltfHierarchyPairs.push_back({childObjIdx, it->second});
            }
        }
    }

    // Create SceneNew for ECS components (lights, meshes, etc.)
    m_sceneNew = std::make_unique<SceneNew>(sceneName);
    
    // Create GameObjects in SceneNew for each mesh Object
    // This allows the editor hierarchy to show all objects
    for (size_t i = 0; i < objs.size(); ++i) {
        Object& obj = objs[i];
        
        // Generate a name for the object
        std::string goName = obj.name.empty() 
            ? ("Object_" + std::to_string(i)) 
            : obj.name;
        
        uint32_t goId = m_sceneNew->CreateGameObject(goName);
        obj.gameObjectId = goId;
        
        // Decompose the object's localTransform matrix into position/rotation/scale
        Transform* t = m_sceneNew->GetTransform(goId);
        if (t) {
            TransformFromMatrix(obj.localTransform, *t);
        }
        
        // Add a RendererComponent to mark this as a mesh object
        RendererComponent renderer;
        renderer.mesh = obj.pMesh;
        renderer.material = obj.pMaterial;
        renderer.texture = obj.pTexture;
        renderer.bVisible = true;
        renderer.matProps.baseColor[0] = obj.color[0];
        renderer.matProps.baseColor[1] = obj.color[1];
        renderer.matProps.baseColor[2] = obj.color[2];
        renderer.matProps.baseColor[3] = obj.color[3];
        renderer.matProps.metallic = obj.metallicFactor;
        renderer.matProps.roughness = obj.roughnessFactor;
        m_sceneNew->AddRenderer(goId, renderer);
    }
    
    // Resolve parent-child relationships from JSON "parent" fields
    // Build name -> gameObjectId map for resolution
    std::unordered_map<std::string, uint32_t> nameToId;
    for (const auto& obj : objs) {
        if (!obj.name.empty() && obj.gameObjectId != UINT32_MAX) {
            nameToId[obj.name] = obj.gameObjectId;
        }
    }
    
    // Set parents based on parsed parent names
    size_t parentSetCount = 0;
    for (size_t i = 0; i < objs.size() && i < instanceParentNames.size(); ++i) {
        const std::string& parentName = instanceParentNames[i];
        if (parentName.empty()) continue;
        
        auto it = nameToId.find(parentName);
        if (it != nameToId.end()) {
            uint32_t childId = objs[i].gameObjectId;
            uint32_t parentId = it->second;
            // Use preserveWorldPosition = false; we compute local transforms manually below
            if (m_sceneNew->SetParent(childId, parentId, false)) {
                ++parentSetCount;
            } else {
                VulkanUtils::LogErr("SceneManager: failed to set parent \"{}\" for object \"{}\" (cycle or invalid)",
                                   parentName, objs[i].name);
            }
        } else {
            VulkanUtils::LogErr("SceneManager: parent \"{}\" not found for object \"{}\"",
                               parentName, objs[i].name);
        }
    }
    
    // Set parents based on glTF internal node hierarchy
    size_t gltfParentSetCount = 0;
    for (const auto& pair : gltfHierarchyPairs) {
        size_t childObjIdx = pair.first;
        size_t parentObjIdx = pair.second;
        
        if (childObjIdx < objs.size() && parentObjIdx < objs.size()) {
            uint32_t childId = objs[childObjIdx].gameObjectId;
            uint32_t parentId = objs[parentObjIdx].gameObjectId;
            
            if (childId != UINT32_MAX && parentId != UINT32_MAX) {
                // Only set if not already parented (JSON parent takes precedence)
                Transform* pChildTransform = m_sceneNew->GetTransform(childId);
                if (pChildTransform && pChildTransform->parentId == NO_PARENT) {
                    // Use preserveWorldPosition = false; we compute local transforms manually below
                    if (m_sceneNew->SetParent(childId, parentId, false)) {
                        ++gltfParentSetCount;
                    }
                }
            }
        }
    }
    
    if (gltfParentSetCount > 0) {
        VulkanUtils::LogInfo("SceneManager: set {} glTF internal parent-child relationships", gltfParentSetCount);
    }
    
    // For objects with parents, recompute local transforms from world transforms
    // Currently transforms are decomposed from baked world matrices, but hierarchy
    // expects local transforms (relative to parent)
    for (auto& obj : objs) {
        if (obj.gameObjectId == UINT32_MAX) continue;
        
        Transform* pTransform = m_sceneNew->GetTransform(obj.gameObjectId);
        if (!pTransform || pTransform->parentId == NO_PARENT) continue;
        
        // Child has a parent - we need to compute local transform
        Transform* pParentTransform = m_sceneNew->GetTransform(pTransform->parentId);
        if (!pParentTransform) continue;
        
        // Get child's current world matrix (stored in obj.localTransform, which is actually baked world)
        glm::mat4 childWorld = glm::make_mat4(obj.localTransform);
        
        // Get parent's world matrix (also baked in their localTransform)
        const GameObject* pParentGO = m_sceneNew->FindGameObject(pTransform->parentId);
        if (!pParentGO) continue;
        
        // Find parent's Object to get its world matrix
        glm::mat4 parentWorld = glm::mat4(1.0f);
        for (const auto& parentObj : objs) {
            if (parentObj.gameObjectId == pTransform->parentId) {
                parentWorld = glm::make_mat4(parentObj.localTransform);
                break;
            }
        }
        
        // Compute local: local = inverse(parentWorld) * childWorld
        glm::mat4 localMatrix = glm::inverse(parentWorld) * childWorld;
        
        // Decompose local matrix into position/rotation/scale
        float localMatrixArr[16];
        std::memcpy(localMatrixArr, glm::value_ptr(localMatrix), sizeof(localMatrixArr));
        TransformFromMatrix(localMatrixArr, *pTransform);
    }
    
    const size_t objectCount = objs.size();
    SetCurrentScene(std::move(scene));
    
    // Load lights from JSON
    LoadLightsFromJson(j);
    
    VulkanUtils::LogInfo("SceneManager: loaded level \"{}\" ({} objects, {} lights)", 
                         path, objectCount, m_sceneNew->GetLights().size());
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

void SceneManager::SyncTransformsToScene() {
    if (!m_currentScene || !m_sceneNew) return;
    
    // First, update all world matrices respecting hierarchy
    m_sceneNew->UpdateWorldMatrices();
    
    auto& objs = m_currentScene->GetObjects();
    const auto& transforms = m_sceneNew->GetTransforms();
    
    // Sync transforms from SceneNew GameObjects back to Scene Objects
    for (auto& obj : objs) {
        if (obj.gameObjectId == UINT32_MAX) continue;
        
#if !EDITOR_BUILD
        // Runtime optimization: Static tier objects never move, skip sync
        // In editor builds, always sync (user may move any object)
        if (obj.instanceTier == InstanceTier::Static) continue;
#endif
        
        // Find the GameObject
        const GameObject* go = m_sceneNew->FindGameObject(obj.gameObjectId);
        if (!go || go->transformIndex >= transforms.size()) continue;
        
        const Transform& t = transforms[go->transformIndex];
        
        // Save old matrix for change detection
        float oldMatrix[16];
        std::memcpy(oldMatrix, obj.localTransform, sizeof(oldMatrix));
        
        // Copy the WORLD matrix to obj.localTransform (used for rendering)
        // This handles hierarchy: child objects use their world transform for GPU
        std::memcpy(obj.localTransform, t.worldMatrix, sizeof(obj.localTransform));
        
#if EDITOR_BUILD
        // In editor: mark dirty if ANY matrix element changed (position, rotation, or scale)
        // This ensures gizmo rotation/scale operations update the GPU buffer
        bool changed = false;
        for (int i = 0; i < 16 && !changed; ++i) {
            if (obj.localTransform[i] != oldMatrix[i]) changed = true;
        }
        if (changed) {
            obj.MarkDirty();
        }
#else
        // Runtime: For SemiStatic tier, mark dirty if transform changed
        // Dynamic tier doesn't need dirty tracking (always uploads)
        if (obj.instanceTier == InstanceTier::SemiStatic) {
            // Quick check: did translation change? (most common case)
            if (obj.localTransform[12] != oldMatrix[12] ||
                obj.localTransform[13] != oldMatrix[13] ||
                obj.localTransform[14] != oldMatrix[14]) {
                obj.MarkDirty();
            }
            // Note: Full rotation/scale change detection could compare all 16 floats,
            // but translation check catches most movement. ECS can also explicitly mark dirty.
        }
#endif
    }
}

void SceneManager::SyncEmissiveLights() {
    if (!m_currentScene || !m_sceneNew) return;
    
    auto& objs = m_currentScene->GetObjects();
    auto& lights = m_sceneNew->GetLights();
    
    for (auto& obj : objs) {
        // Calculate emissive intensity
        float emissiveIntensity = std::sqrt(
            obj.emissive[0] * obj.emissive[0] +
            obj.emissive[1] * obj.emissive[1] +
            obj.emissive[2] * obj.emissive[2]
        ) * obj.emissive[3] * obj.emissiveLightIntensity;
        
        const bool shouldHaveLight = obj.emitsLight && (emissiveIntensity >= 0.001f);
        const bool hasLight = (obj.emissiveLightId != UINT32_MAX);
        
        if (shouldHaveLight && !hasLight) {
            // Create new light for this emissive object
            std::string lightName = obj.name.empty() ? "EmissiveLight" : (obj.name + "_Light");
            uint32_t lightGoId = m_sceneNew->CreateGameObject(lightName);
            
            // Set light position from object's world center
            Transform* lightTransform = m_sceneNew->GetTransform(lightGoId);
            if (lightTransform) {
                // Compute world position from object transform
                const float* m = obj.localTransform;
                float cx = m[12], cy = m[13], cz = m[14]; // Default: translation
                
                if (obj.pMesh != nullptr) {
                    const MeshAABB& aabb = obj.pMesh->GetAABB();
                    if (aabb.IsValid()) {
                        float localCx, localCy, localCz;
                        aabb.GetCenter(localCx, localCy, localCz);
                        // Transform center to world space
                        cx = m[0]*localCx + m[4]*localCy + m[8]*localCz + m[12];
                        cy = m[1]*localCx + m[5]*localCy + m[9]*localCz + m[13];
                        cz = m[2]*localCx + m[6]*localCy + m[10]*localCz + m[14];
                    }
                }
                
                TransformSetPosition(*lightTransform, cx, cy, cz);
            }
            
            // Create light component
            LightComponent light;
            light.type = LightType::Point;
            light.color[0] = obj.emissive[0];
            light.color[1] = obj.emissive[1];
            light.color[2] = obj.emissive[2];
            light.intensity = obj.emissive[3] * obj.emissiveLightIntensity;
            light.range = obj.emissiveLightRadius;
            light.falloffExponent = 2.0f;  // Physically correct inverse square
            light.bActive = true;
            
            m_sceneNew->AddLight(lightGoId, light);
            obj.emissiveLightId = lightGoId;
            
            VulkanUtils::LogTrace("SyncEmissiveLights: Created light for object '{}' (lightGoId={})", 
                                  obj.name, lightGoId);
            
        } else if (shouldHaveLight && hasLight) {
            // Update existing light
            GameObject* lightGo = m_sceneNew->FindGameObject(obj.emissiveLightId);
            if (lightGo && lightGo->lightIndex < lights.size()) {
                LightComponent& light = lights[lightGo->lightIndex];
                
                // Update light properties
                light.color[0] = obj.emissive[0];
                light.color[1] = obj.emissive[1];
                light.color[2] = obj.emissive[2];
                light.intensity = obj.emissive[3] * obj.emissiveLightIntensity;
                light.range = obj.emissiveLightRadius;
                
                // Update position from object's world center
                Transform* lightTransform = m_sceneNew->GetTransform(obj.emissiveLightId);
                if (lightTransform) {
                    const float* m = obj.localTransform;
                    float cx = m[12], cy = m[13], cz = m[14];
                    
                    if (obj.pMesh != nullptr) {
                        const MeshAABB& aabb = obj.pMesh->GetAABB();
                        if (aabb.IsValid()) {
                            float localCx, localCy, localCz;
                            aabb.GetCenter(localCx, localCy, localCz);
                            cx = m[0]*localCx + m[4]*localCy + m[8]*localCz + m[12];
                            cy = m[1]*localCx + m[5]*localCy + m[9]*localCz + m[13];
                            cz = m[2]*localCx + m[6]*localCy + m[10]*localCz + m[14];
                        }
                    }
                    
                    TransformSetPosition(*lightTransform, cx, cy, cz);
                }
            }
            
        } else if (!shouldHaveLight && hasLight) {
            // Remove light (object no longer emits light)
            m_sceneNew->DestroyGameObject(obj.emissiveLightId);
            obj.emissiveLightId = UINT32_MAX;
            
            VulkanUtils::LogTrace("SyncEmissiveLights: Removed light for object '{}'", obj.name);
        }
        // else: !shouldHaveLight && !hasLight - nothing to do
    }
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

void SceneManager::LoadLightsFromJson(const nlohmann::json& j) {
    if (!m_sceneNew) return;
    
    if (!j.contains("lights") || !j["lights"].is_array()) {
        // No lights in the level - add a default ambient-ish directional light
        uint32_t goId = m_sceneNew->CreateGameObject("DefaultSun");
        Transform* t = m_sceneNew->GetTransform(goId);
        if (t) {
            TransformSetPosition(*t, 0.f, 10.f, 0.f);
            // Point slightly downward (rotation around X axis by ~30 degrees)
            TransformSetRotation(*t, 0.259f, 0.f, 0.f, 0.966f);
        }
        
        LightComponent light;
        light.type = LightType::Directional;
        light.color[0] = 1.f; light.color[1] = 1.f; light.color[2] = 1.f;
        light.intensity = 1.5f;
        m_sceneNew->AddLight(goId, light);
        
        VulkanUtils::LogInfo("SceneManager: no lights in level, created default directional light");
        return;
    }
    
    for (const auto& jLight : j["lights"]) {
        if (!jLight.is_object()) continue;
        
        // Get light name
        std::string name = "Light";
        if (jLight.contains("name") && jLight["name"].is_string()) {
            name = jLight["name"].get<std::string>();
        }
        
        // Create GameObject for this light
        uint32_t goId = m_sceneNew->CreateGameObject(name);
        Transform* t = m_sceneNew->GetTransform(goId);
        
        // Parse position
        if (t && jLight.contains("position") && jLight["position"].is_array() && jLight["position"].size() >= 3) {
            float px = static_cast<float>(jLight["position"][0].get<double>());
            float py = static_cast<float>(jLight["position"][1].get<double>());
            float pz = static_cast<float>(jLight["position"][2].get<double>());
            TransformSetPosition(*t, px, py, pz);
        }
        
        // Parse rotation (quaternion: x, y, z, w)
        if (t && jLight.contains("rotation") && jLight["rotation"].is_array() && jLight["rotation"].size() >= 4) {
            float qx = static_cast<float>(jLight["rotation"][0].get<double>());
            float qy = static_cast<float>(jLight["rotation"][1].get<double>());
            float qz = static_cast<float>(jLight["rotation"][2].get<double>());
            float qw = static_cast<float>(jLight["rotation"][3].get<double>());
            TransformSetRotation(*t, qx, qy, qz, qw);
        }
        
        // Parse light properties
        LightComponent light;
        
        // Type
        if (jLight.contains("type") && jLight["type"].is_string()) {
            std::string typeStr = jLight["type"].get<std::string>();
            if (typeStr == "directional" || typeStr == "Directional") {
                light.type = LightType::Directional;
            } else if (typeStr == "point" || typeStr == "Point") {
                light.type = LightType::Point;
            } else if (typeStr == "spot" || typeStr == "Spot") {
                light.type = LightType::Spot;
            } else {
                VulkanUtils::LogWarn("SceneManager: unknown light type \"{}\" for \"{}\", defaulting to point",
                                     typeStr, name);
                light.type = LightType::Point;
            }
        }
        
        // Color
        if (jLight.contains("color") && jLight["color"].is_array() && jLight["color"].size() >= 3) {
            light.color[0] = static_cast<float>(jLight["color"][0].get<double>());
            light.color[1] = static_cast<float>(jLight["color"][1].get<double>());
            light.color[2] = static_cast<float>(jLight["color"][2].get<double>());
        }
        
        // Intensity
        if (jLight.contains("intensity") && jLight["intensity"].is_number()) {
            light.intensity = static_cast<float>(jLight["intensity"].get<double>());
        }
        
        // Range (for point and spot lights)
        if (jLight.contains("range") && jLight["range"].is_number()) {
            light.range = static_cast<float>(jLight["range"].get<double>());
        }
        
        // Cone angles (for spot lights)
        if (jLight.contains("innerConeAngle") && jLight["innerConeAngle"].is_number()) {
            light.innerConeAngle = static_cast<float>(jLight["innerConeAngle"].get<double>());
        }
        if (jLight.contains("outerConeAngle") && jLight["outerConeAngle"].is_number()) {
            light.outerConeAngle = static_cast<float>(jLight["outerConeAngle"].get<double>());
        }
        
        // Falloff exponent
        if (jLight.contains("falloff") && jLight["falloff"].is_number()) {
            light.falloffExponent = static_cast<float>(jLight["falloff"].get<double>());
        }
        
        // Active flag
        if (jLight.contains("active") && jLight["active"].is_boolean()) {
            light.bActive = jLight["active"].get<bool>();
        }
        
        // Cast shadows (future)
        if (jLight.contains("castShadows") && jLight["castShadows"].is_boolean()) {
            light.bCastShadows = jLight["castShadows"].get<bool>();
        }
        
        m_sceneNew->AddLight(goId, light);
        
        const char* typeStr = (light.type == LightType::Directional) ? "Directional" :
                              (light.type == LightType::Point) ? "Point" :
                              (light.type == LightType::Spot) ? "Spot" : "Unknown";
        VulkanUtils::LogInfo("Light[{}]: {} \"{}\" pos=({:.1f}, {:.1f}, {:.1f}) color=({:.2f}, {:.2f}, {:.2f}) intensity={:.2f} range={:.1f}",
                             goId, typeStr, name,
                             t ? t->position[0] : 0.f,
                             t ? t->position[1] : 0.f,
                             t ? t->position[2] : 0.f,
                             light.color[0], light.color[1], light.color[2],
                             light.intensity, light.range);
    }
    
    // Summary
    const auto& lights = m_sceneNew->GetLights();
    uint32_t pointCount = 0, spotCount = 0, dirCount = 0;
    for (const auto& l : lights) {
        if (!l.bActive) continue;
        switch (l.type) {
            case LightType::Point: ++pointCount; break;
            case LightType::Spot: ++spotCount; break;
            case LightType::Directional: ++dirCount; break;
            default: break;
        }
    }
    VulkanUtils::LogInfo("Lights loaded: {} directional, {} point, {} spot (total: {})",
                         dirCount, pointCount, spotCount, lights.size());
}

uint32_t SceneManager::GenerateStressTestScene(const StressTestParams& params, const std::string& modelPath) {
    if (!m_pMaterialManager || !m_pMeshManager || !m_pTextureManager) {
        VulkanUtils::LogErr("SceneManager: dependencies not set for GenerateStressTestScene");
        return 0;
    }
    
    // Load the glTF model
    if (!m_gltfLoader.LoadFromFile(modelPath)) {
        VulkanUtils::LogErr("SceneManager: failed to load glTF model: {}", modelPath);
        return 0;
    }
    
    const tinygltf::Model* pModel = m_gltfLoader.GetModel();
    if (!pModel || pModel->meshes.empty()) {
        VulkanUtils::LogErr("SceneManager: glTF model has no meshes: {}", modelPath);
        return 0;
    }
    
    // Create new scene
    UnloadScene();
    m_currentScene = std::make_unique<Scene>();
    m_currentScene->SetName("Stress Test");
    m_sceneNew = std::make_unique<SceneNew>();
    
    // Extract first mesh/primitive from model
    const tinygltf::Mesh& mesh = pModel->meshes[0];
    if (mesh.primitives.empty()) {
        VulkanUtils::LogErr("SceneManager: glTF mesh has no primitives: {}", modelPath);
        return 0;
    }
    
    const tinygltf::Primitive& prim = mesh.primitives[0];
    
    // Get material
    std::shared_ptr<MaterialHandle> pMaterial;
    std::shared_ptr<TextureHandle> pTexture;
    if (prim.material >= 0 && size_t(prim.material) < pModel->materials.size()) {
        const tinygltf::Material& gltfMat = pModel->materials[size_t(prim.material)];
        const bool hasTexture = (gltfMat.pbrMetallicRoughness.baseColorTexture.index >= 0);
        const bool doubleSided = gltfMat.doubleSided;
        const std::string pipelineKey = hasTexture ? (doubleSided ? "main_tex_ds" : "main_tex") : (doubleSided ? "main_untex_ds" : "main_untex");
        pMaterial = m_pMaterialManager->GetMaterial(pipelineKey);
        
        // Load texture
        if (hasTexture) {
            int texIdx = gltfMat.pbrMetallicRoughness.baseColorTexture.index;
            if (texIdx >= 0 && size_t(texIdx) < pModel->textures.size()) {
                const tinygltf::Texture& tex = pModel->textures[size_t(texIdx)];
                if (tex.source >= 0 && size_t(tex.source) < pModel->images.size()) {
                    const tinygltf::Image& img = pModel->images[size_t(tex.source)];
                    if (!img.image.empty() && img.width > 0 && img.height > 0) {
                        std::string texName = "stress_test_tex";
                        pTexture = m_pTextureManager->GetOrCreateFromMemory(texName, img.width, img.height, img.component, img.image.data());
                    }
                }
            }
        }
    }
    
    if (!pMaterial) {
        pMaterial = m_pMaterialManager->GetMaterial("main_tex");
        if (!pMaterial) {
            VulkanUtils::LogErr("SceneManager: no valid material for stress test");
            return 0;
        }
    }
    
    // Load mesh data
    std::vector<VertexData> vertices;
    if (!GetMeshDataFromGltf(*pModel, 0, 0, vertices)) {
        VulkanUtils::LogErr("SceneManager: failed to extract mesh data from glTF");
        return 0;
    }
    
    const uint32_t vertexCount = static_cast<uint32_t>(vertices.size());
    const std::string meshKey = "stress_test_mesh";
    std::shared_ptr<MeshHandle> pMesh = m_pMeshManager->GetOrCreateFromGltf(meshKey, vertices.data(), vertexCount);
    if (!pMesh) {
        VulkanUtils::LogErr("SceneManager: failed to create mesh for stress test");
        return 0;
    }
    
    // Random number generator (simple xorshift)
    uint32_t rngState = params.seed ? params.seed : 12345;
    auto nextRandom = [&rngState]() -> uint32_t {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        return rngState;
    };
    auto nextFloat = [&nextRandom]() -> float {
        return static_cast<float>(nextRandom()) / static_cast<float>(0xFFFFFFFFu);
    };
    auto nextFloatRange = [&nextFloat](float minV, float maxV) -> float {
        return minV + nextFloat() * (maxV - minV);
    };
    
    uint32_t totalCount = GetStressTestObjectCount(params);
    uint32_t created = 0;
    
    // Create objects for each tier
    auto createObjects = [&](uint32_t count, InstanceTier tier, const char* namePrefix) {
        for (uint32_t i = 0; i < count && created < totalCount; ++i) {
            Object obj;
            obj.name = std::string(namePrefix) + "_" + std::to_string(i);
            obj.instanceTier = tier;
            obj.pMesh = pMesh;
            obj.pMaterial = pMaterial;
            obj.pTexture = pTexture;
            
            // Random position
            float px = nextFloatRange(-params.worldSize, params.worldSize);
            float py = nextFloatRange(0.0f, params.heightVariation);
            float pz = nextFloatRange(-params.worldSize, params.worldSize);
            
            // Random Y rotation
            float angle = nextFloat() * 6.28318f;
            float qx = 0.0f, qy = std::sin(angle * 0.5f), qz = 0.0f, qw = std::cos(angle * 0.5f);
            
            // Random scale
            float scale = params.randomScales ? nextFloatRange(params.minScale, params.maxScale) : 1.0f;
            
            // Random color
            if (params.randomColors) {
                float h = nextFloat() * 6.0f;
                float s = nextFloatRange(0.6f, 1.0f);
                float v = nextFloatRange(0.5f, 1.0f);
                int hi = static_cast<int>(h);
                float f = h - static_cast<float>(hi);
                float p = v * (1.0f - s);
                float q = v * (1.0f - s * f);
                float t = v * (1.0f - s * (1.0f - f));
                switch (hi % 6) {
                    case 0: obj.color[0] = v; obj.color[1] = t; obj.color[2] = p; break;
                    case 1: obj.color[0] = q; obj.color[1] = v; obj.color[2] = p; break;
                    case 2: obj.color[0] = p; obj.color[1] = v; obj.color[2] = t; break;
                    case 3: obj.color[0] = p; obj.color[1] = q; obj.color[2] = v; break;
                    case 4: obj.color[0] = t; obj.color[1] = p; obj.color[2] = v; break;
                    default: obj.color[0] = v; obj.color[1] = p; obj.color[2] = q; break;
                }
                obj.color[3] = 1.0f;
            } else {
                obj.color[0] = obj.color[1] = obj.color[2] = obj.color[3] = 1.0f;
            }
            
            ObjectSetFromPositionRotationScale(obj.localTransform, px, py, pz, qx, qy, qz, qw, scale, scale, scale);
            m_currentScene->AddObject(std::move(obj));
            ++created;
        }
    };
    
    createObjects(params.staticCount, InstanceTier::Static, "static");
    createObjects(params.semiStaticCount, InstanceTier::SemiStatic, "semistatic");
    createObjects(params.dynamicCount, InstanceTier::Dynamic, "dynamic");
    createObjects(params.proceduralCount, InstanceTier::Procedural, "procedural");
    
    VulkanUtils::LogInfo("Stress test generated: {} objects from {}", created, modelPath);
    return created;
}
