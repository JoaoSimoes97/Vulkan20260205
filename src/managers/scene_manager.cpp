/*
 * SceneManager — level = JSON + many glTFs; LoadLevelFromFile, EnsureDefaultLevelFile, LoadDefaultLevelOrCreate.
 */
#include "scene_manager.h"
#include "material_manager.h"
#include "mesh_manager.h"
#include "texture_manager.h"
#include "scene/object.h"
#include "core/transform.h"
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

// ============================================================================
// File-scoped glTF model cache (avoids incomplete type issues in header)
// ============================================================================
static std::map<std::string, std::unique_ptr<tinygltf::Model>> s_gltfModelCache;

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
    m_proceduralMeshCache.clear();
    ClearGltfCache();
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

    std::vector<Object> objs;

    std::vector<std::string> instanceParentNames;
    std::vector<std::pair<size_t, size_t>> gltfHierarchyPairs;

    // ========================================================================
    // Parse model definitions (reusable templates for instances)
    // ========================================================================
    struct ModelDef {
        std::string source;
        std::string renderMode = "auto";
        std::string instanceTier = "static";
    };
    std::map<std::string, ModelDef> modelDefs;
    
    if (j.contains("models") && j["models"].is_object()) {
        for (auto& [modelName, modelJson] : j["models"].items()) {
            if (!modelJson.is_object()) continue;
            
            ModelDef def;
            if (modelJson.contains("source") && modelJson["source"].is_string()) {
                def.source = modelJson["source"].get<std::string>();
            }
            if (modelJson.contains("renderMode") && modelJson["renderMode"].is_string()) {
                def.renderMode = modelJson["renderMode"].get<std::string>();
            }
            if (modelJson.contains("instanceTier") && modelJson["instanceTier"].is_string()) {
                def.instanceTier = modelJson["instanceTier"].get<std::string>();
            }
            
            if (!def.source.empty()) {
                modelDefs[modelName] = def;
                VulkanUtils::LogInfo("SceneManager: registered model definition \"{}\" -> \"{}\"", 
                                     modelName, def.source);
            }
        }
    }

    if (!j.contains("instances") || !j["instances"].is_array()) {
        SetCurrentScene(std::make_unique<Scene>(sceneName));
        VulkanUtils::LogInfo("SceneManager: loaded level \"{}\" (no instances)", path);
        return true;
    }

    for (const auto& jInst : j["instances"]) {
        if (!jInst.is_object())
            continue;
        
        // Resolve source and defaults from model definition or direct specification
        std::string source;
        std::string defaultRenderMode = "auto";
        std::string defaultInstanceTier = "static";
        
        // Check for model reference first (new format)
        if (jInst.contains("model") && jInst["model"].is_string()) {
            const std::string modelRef = jInst["model"].get<std::string>();
            auto it = modelDefs.find(modelRef);
            if (it != modelDefs.end()) {
                source = it->second.source;
                defaultRenderMode = it->second.renderMode;
                defaultInstanceTier = it->second.instanceTier;
            } else {
                VulkanUtils::LogErr("SceneManager: unknown model reference \"{}\"", modelRef);
                continue;
            }
        }
        // Fall back to direct source (legacy format)
        else if (jInst.contains("source") && jInst["source"].is_string()) {
            source = jInst["source"].get<std::string>();
        }
        else {
            continue; // Neither model nor source specified
        }
        
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

        // Parse renderMode (instance override takes precedence over model default)
        RenderMode renderMode = RenderMode::Auto;
        std::string modeStr = defaultRenderMode;
        if (jInst.contains("renderMode") && jInst["renderMode"].is_string()) {
            modeStr = jInst["renderMode"].get<std::string>();
        }
        if (modeStr == "solid") renderMode = RenderMode::Solid;
        else if (modeStr == "wireframe") renderMode = RenderMode::Wireframe;
        else if (modeStr == "auto") renderMode = RenderMode::Auto;
        else {
            VulkanUtils::LogErr("SceneManager: unknown renderMode \"{}\" for source \"{}\"", modeStr, source);
            continue;
        }

        // Parse instance tier (instance override takes precedence over model default)
        std::string tierStr = defaultInstanceTier;
        if (jInst.contains("instanceTier") && jInst["instanceTier"].is_string()) {
            tierStr = jInst["instanceTier"].get<std::string>();
        }
        InstanceTier instanceTier = ParseInstanceTier(tierStr);

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
        
        // Use cached model loading (avoids re-parsing same file for multiple instances)
        const tinygltf::Model* model = GetOrLoadGltfModel(gltfPath);
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

    // Build unified Scene from parsed Objects
    auto scene = std::make_unique<Scene>(sceneName);
    std::vector<uint32_t> goIds(objs.size(), UINT32_MAX);

    for (size_t i = 0; i < objs.size(); ++i) {
        Object& obj = objs[i];
        std::string goName = obj.name.empty() ? ("Object_" + std::to_string(i)) : obj.name;
        uint32_t goId = scene->CreateGameObject(goName);
        goIds[i] = goId;

        Transform t;
        ObjectToTransform(obj, t);
        scene->AddTransform(goId, t);

        RendererComponent renderer;
        ObjectToRenderer(obj, renderer);
        scene->AddRenderer(goId, renderer);
    }

    // Resolve parent-child from JSON "parent" names
    std::unordered_map<std::string, uint32_t> nameToId;
    for (size_t i = 0; i < objs.size(); ++i) {
        if (!objs[i].name.empty() && goIds[i] != UINT32_MAX)
            nameToId[objs[i].name] = goIds[i];
    }
    for (size_t i = 0; i < objs.size() && i < instanceParentNames.size(); ++i) {
        const std::string& parentName = instanceParentNames[i];
        if (parentName.empty()) continue;
        auto it = nameToId.find(parentName);
        if (it != nameToId.end()) {
            uint32_t childId = goIds[i], parentId = it->second;
            if (!scene->SetParent(childId, parentId, true))
                VulkanUtils::LogErr("SceneManager: failed to set parent \"{}\" for object \"{}\"", parentName, objs[i].name);
        } else {
            VulkanUtils::LogErr("SceneManager: parent \"{}\" not found for object \"{}\"", parentName, objs[i].name);
        }
    }

    // Set parents from glTF internal hierarchy
    for (const auto& pair : gltfHierarchyPairs) {
        size_t childObjIdx = pair.first, parentObjIdx = pair.second;
        if (childObjIdx >= goIds.size() || parentObjIdx >= goIds.size()) continue;
        uint32_t childId = goIds[childObjIdx], parentId = goIds[parentObjIdx];
        if (childId == UINT32_MAX || parentId == UINT32_MAX) continue;
        const Transform* pChild = scene->GetTransform(childId);
        if (pChild && pChild->parentId == NO_PARENT)
            scene->SetParent(childId, parentId, true);
    }

    const size_t objectCount = objs.size();
    SetCurrentScene(std::move(scene));
    LoadLightsFromJson(j);

    VulkanUtils::LogInfo("SceneManager: loaded level \"{}\" ({} objects, {} lights)",
                         path, objectCount, m_currentScene ? m_currentScene->GetLights().size() : 0u);
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
    if (!m_currentScene) return;
    uint32_t goId = m_currentScene->CreateGameObject(obj.name.empty() ? "Object" : obj.name);
    Transform t;
    ObjectToTransform(obj, t);
    m_currentScene->AddTransform(goId, t);
    RendererComponent r;
    ObjectToRenderer(obj, r);
    m_currentScene->AddRenderer(goId, r);
}

void SceneManager::RemoveObject(size_t index) {
    if (!m_currentScene) return;
    const auto& gameObjects = m_currentScene->GetGameObjects();
    size_t renderableIdx = 0;
    for (const auto& go : gameObjects) {
        if (!go.HasRenderer()) continue;
        if (renderableIdx == index) {
            m_currentScene->DestroyGameObject(go.id);
            return;
        }
        ++renderableIdx;
    }
}

void SceneManager::ObjectToTransform(const Object& obj, Transform& out) {
    TransformFromMatrix(obj.localTransform, out);
}

void SceneManager::ObjectToRenderer(const Object& obj, RendererComponent& out) {
    out.mesh = obj.pMesh;
    out.material = obj.pMaterial;
    out.texture = obj.pTexture;
    out.pMetallicRoughnessTexture = obj.pMetallicRoughnessTexture;
    out.pEmissiveTexture = obj.pEmissiveTexture;
    out.pNormalTexture = obj.pNormalTexture;
    out.pOcclusionTexture = obj.pOcclusionTexture;
    out.matProps.baseColor[0] = obj.color[0];
    out.matProps.baseColor[1] = obj.color[1];
    out.matProps.baseColor[2] = obj.color[2];
    out.matProps.baseColor[3] = obj.color[3];
    out.matProps.emissive[0] = obj.emissive[0];
    out.matProps.emissive[1] = obj.emissive[1];
    out.matProps.emissive[2] = obj.emissive[2];
    out.matProps.emissive[3] = obj.emissive[3];
    out.matProps.metallic = obj.metallicFactor;
    out.matProps.roughness = obj.roughnessFactor;
    out.bVisible = true;
    out.emitsLight = obj.emitsLight;
    out.emissiveLightRadius = obj.emissiveLightRadius;
    out.emissiveLightIntensity = obj.emissiveLightIntensity;
    out.instanceTier = static_cast<uint8_t>(static_cast<std::underlying_type_t<InstanceTier>>(obj.instanceTier));
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

const tinygltf::Model* SceneManager::GetOrLoadGltfModel(const std::string& path) {
    // Check cache first
    auto it = s_gltfModelCache.find(path);
    if (it != s_gltfModelCache.end()) {
        return it->second.get();
    }
    
    // Load from file
    if (!m_gltfLoader.LoadFromFile(path)) {
        VulkanUtils::LogErr("SceneManager: failed to load glTF \"{}\"", path);
        return nullptr;
    }
    
    // Clone the model into cache (GltfLoader owns one model at a time)
    const tinygltf::Model* pLoaded = m_gltfLoader.GetModel();
    if (!pLoaded) {
        return nullptr;
    }
    
    // Move-construct a copy into the cache
    auto pCached = std::make_unique<tinygltf::Model>(*pLoaded);
    const tinygltf::Model* pResult = pCached.get();
    s_gltfModelCache[path] = std::move(pCached);
    
    VulkanUtils::LogInfo("SceneManager: cached glTF \"{}\" ({} meshes, {} materials)",
                         path, pResult->meshes.size(), pResult->materials.size());
    
    return pResult;
}

void SceneManager::ClearGltfCache() {
    if (!s_gltfModelCache.empty()) {
        VulkanUtils::LogInfo("SceneManager: cleared {} cached glTF models", s_gltfModelCache.size());
        s_gltfModelCache.clear();
    }
}

void SceneManager::LoadLightsFromJson(const nlohmann::json& j) {
    if (!m_currentScene) return;

    if (!j.contains("lights") || !j["lights"].is_array()) {
        uint32_t goId = m_currentScene->CreateGameObject("DefaultSun");
        Transform t;
        TransformSetPosition(t, 0.f, 10.f, 0.f);
        TransformSetRotation(t, 0.259f, 0.f, 0.f, 0.966f);
        m_currentScene->AddTransform(goId, t);

        LightComponent light;
        light.type = LightType::Directional;
        light.color[0] = 1.f; light.color[1] = 1.f; light.color[2] = 1.f;
        light.intensity = 1.5f;
        m_currentScene->AddLight(goId, light);

        VulkanUtils::LogInfo("SceneManager: no lights in level, created default directional light");
        return;
    }

    for (const auto& jLight : j["lights"]) {
        if (!jLight.is_object()) continue;

        std::string name = "Light";
        if (jLight.contains("name") && jLight["name"].is_string()) {
            name = jLight["name"].get<std::string>();
        }

        uint32_t goId = m_currentScene->CreateGameObject(name);
        Transform t;

        // Parse position
        if (jLight.contains("position") && jLight["position"].is_array() && jLight["position"].size() >= 3) {
            float px = static_cast<float>(jLight["position"][0].get<double>());
            float py = static_cast<float>(jLight["position"][1].get<double>());
            float pz = static_cast<float>(jLight["position"][2].get<double>());
            TransformSetPosition(t, px, py, pz);
        }

        if (jLight.contains("rotation") && jLight["rotation"].is_array() && jLight["rotation"].size() >= 4) {
            float qx = static_cast<float>(jLight["rotation"][0].get<double>());
            float qy = static_cast<float>(jLight["rotation"][1].get<double>());
            float qz = static_cast<float>(jLight["rotation"][2].get<double>());
            float qw = static_cast<float>(jLight["rotation"][3].get<double>());
            TransformSetRotation(t, qx, qy, qz, qw);
        }
        m_currentScene->AddTransform(goId, t);
        
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
        
        m_currentScene->AddLight(goId, light);

        const char* typeStr = (light.type == LightType::Directional) ? "Directional" :
                              (light.type == LightType::Point) ? "Point" :
                              (light.type == LightType::Spot) ? "Spot" : "Unknown";
        VulkanUtils::LogInfo("Light[{}]: {} \"{}\" pos=({:.1f}, {:.1f}, {:.1f}) color=({:.2f}, {:.2f}, {:.2f}) intensity={:.2f} range={:.1f}",
                             goId, typeStr, name,
                             t.position[0], t.position[1], t.position[2],
                             light.color[0], light.color[1], light.color[2],
                             light.intensity, light.range);
    }

    const auto& lights = m_currentScene->GetLights();
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
    
    UnloadScene();
    m_currentScene = std::make_unique<Scene>("Stress Test");
    
    // Load the glTF model (uses cache)
    const tinygltf::Model* pModel = GetOrLoadGltfModel(modelPath);
    if (!pModel || pModel->meshes.empty()) {
        VulkanUtils::LogErr("SceneManager: glTF model has no meshes: {}", modelPath);
        return 0;
    }
    
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
            AddObject(std::move(obj));
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
