/*
 * Extract vertex data (position + UV + normal) from tinygltf meshes for engine upload.
 */
#include "gltf_mesh_utils.h"
#include <tiny_gltf.h>
#include <cstring>
#include <iostream>

namespace {

/**
 * Read VEC3 float data from accessor (e.g., POSITION or NORMAL).
 */
bool GetAccessorDataAsFloat3(const tinygltf::Model& model, int accessorIndex,
                             std::vector<float>& outData) {
    if (accessorIndex < 0 || size_t(accessorIndex) >= model.accessors.size())
        return false;
    const tinygltf::Accessor& acc = model.accessors[size_t(accessorIndex)];
    if (acc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || acc.type != TINYGLTF_TYPE_VEC3)
        return false;
    if (acc.bufferView < 0 || size_t(acc.bufferView) >= model.bufferViews.size())
        return false;
    const tinygltf::BufferView& bv = model.bufferViews[size_t(acc.bufferView)];
    if (bv.buffer < 0 || size_t(bv.buffer) >= model.buffers.size())
        return false;
    const tinygltf::Buffer& buf = model.buffers[size_t(bv.buffer)];
    const size_t compSize = sizeof(float);
    const size_t numComp = 3u;
    const size_t stride = (bv.byteStride > 0) ? size_t(bv.byteStride) : (compSize * numComp);
    const size_t offset = size_t(bv.byteOffset) + size_t(acc.byteOffset);
    const size_t totalBytes = size_t(acc.count) * stride;
    if (offset + totalBytes > buf.data.size())
        return false;
    outData.resize(size_t(acc.count) * numComp);
    const unsigned char* src = buf.data.data() + offset;
    for (size_t i = 0; i < size_t(acc.count); ++i) {
        std::memcpy(&outData[i * numComp], src + i * stride, compSize * numComp);
    }
    return true;
}

/**
 * Read VEC2 float data from accessor (e.g., TEXCOORD_0).
 */
bool GetAccessorDataAsFloat2(const tinygltf::Model& model, int accessorIndex,
                             std::vector<float>& outData) {
    if (accessorIndex < 0 || size_t(accessorIndex) >= model.accessors.size())
        return false;
    const tinygltf::Accessor& acc = model.accessors[size_t(accessorIndex)];
    if (acc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || acc.type != TINYGLTF_TYPE_VEC2)
        return false;
    if (acc.bufferView < 0 || size_t(acc.bufferView) >= model.bufferViews.size())
        return false;
    const tinygltf::BufferView& bv = model.bufferViews[size_t(acc.bufferView)];
    if (bv.buffer < 0 || size_t(bv.buffer) >= model.buffers.size())
        return false;
    const tinygltf::Buffer& buf = model.buffers[size_t(bv.buffer)];
    const size_t compSize = sizeof(float);
    const size_t numComp = 2u;
    const size_t stride = (bv.byteStride > 0) ? size_t(bv.byteStride) : (compSize * numComp);
    const size_t offset = size_t(bv.byteOffset) + size_t(acc.byteOffset);
    const size_t totalBytes = size_t(acc.count) * stride;
    if (offset + totalBytes > buf.data.size())
        return false;
    outData.resize(size_t(acc.count) * numComp);
    const unsigned char* src = buf.data.data() + offset;
    for (size_t i = 0; i < size_t(acc.count); ++i) {
        std::memcpy(&outData[i * numComp], src + i * stride, compSize * numComp);
    }
    return true;
}

/**
 * Read index data (UNSIGNED_SHORT or UNSIGNED_INT).
 */
bool GetIndexDataAsU32(const tinygltf::Model& model, int accessorIndex, std::vector<uint32_t>& outIndices) {
    if (accessorIndex < 0 || size_t(accessorIndex) >= model.accessors.size())
        return false;
    const tinygltf::Accessor& acc = model.accessors[size_t(accessorIndex)];
    if (acc.bufferView < 0 || size_t(acc.bufferView) >= model.bufferViews.size())
        return false;
    const tinygltf::BufferView& bv = model.bufferViews[size_t(acc.bufferView)];
    if (bv.buffer < 0 || size_t(bv.buffer) >= model.buffers.size())
        return false;
    const tinygltf::Buffer& buf = model.buffers[size_t(bv.buffer)];
    const size_t offset = size_t(bv.byteOffset) + size_t(acc.byteOffset);
    outIndices.resize(size_t(acc.count));
    if (acc.componentType == 5125) { /* UNSIGNED_INT */
        if (offset + acc.count * sizeof(uint32_t) > buf.data.size()) return false;
        std::memcpy(outIndices.data(), buf.data.data() + offset, acc.count * sizeof(uint32_t));
    } else if (acc.componentType == 5123) { /* UNSIGNED_SHORT */
        if (offset + acc.count * sizeof(uint16_t) > buf.data.size()) return false;
        const uint16_t* src = reinterpret_cast<const uint16_t*>(buf.data.data() + offset);
        for (size_t i = 0; i < size_t(acc.count); ++i)
            outIndices[i] = uint32_t(src[i]);
    } else {
        return false;
    }
    return true;
}

} // namespace

bool GetMeshDataFromGltf(const tinygltf::Model& model, int meshIndex, int primitiveIndex,
                         std::vector<VertexData>& outVertices) {
    outVertices.clear();
    if (meshIndex < 0 || size_t(meshIndex) >= model.meshes.size())
        return false;
    const tinygltf::Mesh& mesh = model.meshes[size_t(meshIndex)];
    if (primitiveIndex < 0 || size_t(primitiveIndex) >= mesh.primitives.size())
        return false;
    const tinygltf::Primitive& prim = mesh.primitives[size_t(primitiveIndex)];

    // POSITION is required
    auto itPos = prim.attributes.find("POSITION");
    if (itPos == prim.attributes.end()) {
        std::cerr << "[GetMeshDataFromGltf] Missing POSITION attribute.\n";
        return false;
    }
    std::vector<float> positions;
    if (!GetAccessorDataAsFloat3(model, itPos->second, positions)) {
        std::cerr << "[GetMeshDataFromGltf] Failed to read POSITION data.\n";
        return false;
    }
    const size_t vertexCount = positions.size() / 3u;
    if (vertexCount == 0)
        return false;

    // TEXCOORD_0 (UV) is optional, default to (0,0)
    std::vector<float> uvs;
    auto itUV = prim.attributes.find("TEXCOORD_0");
    if (itUV != prim.attributes.end() && GetAccessorDataAsFloat2(model, itUV->second, uvs)) {
        if (uvs.size() / 2u != vertexCount) {
            std::cerr << "[GetMeshDataFromGltf] UV count mismatch, ignoring UVs.\n";
            uvs.clear();
        }
    }
    const bool hasUVs = !uvs.empty();

    // NORMAL is optional, default to (0,0,1)
    std::vector<float> normals;
    auto itNorm = prim.attributes.find("NORMAL");
    if (itNorm != prim.attributes.end() && GetAccessorDataAsFloat3(model, itNorm->second, normals)) {
        if (normals.size() / 3u != vertexCount) {
            std::cerr << "[GetMeshDataFromGltf] Normal count mismatch, ignoring normals.\n";
            normals.clear();
        }
    }
    const bool hasNormals = !normals.empty();

    // Read indices (if any)
    std::vector<uint32_t> indices;
    const bool indexed = (prim.indices >= 0);
    if (indexed) {
        if (!GetIndexDataAsU32(model, prim.indices, indices)) {
            std::cerr << "[GetMeshDataFromGltf] Failed to read index data.\n";
            return false;
        }
    } else {
        // Non-indexed: generate trivial indices
        indices.resize(vertexCount);
        for (size_t i = 0; i < vertexCount; ++i)
            indices[i] = static_cast<uint32_t>(i);
    }

    // Build interleaved vertex data
    outVertices.reserve(indices.size());
    for (uint32_t idx : indices) {
        if (idx >= vertexCount) {
            std::cerr << "[GetMeshDataFromGltf] Index " << idx << " out of range.\n";
            continue;
        }
        VertexData v;
        v.position[0] = positions[idx * 3 + 0];
        v.position[1] = positions[idx * 3 + 1];
        v.position[2] = positions[idx * 3 + 2];

        if (hasUVs) {
            v.uv[0] = uvs[idx * 2 + 0];
            v.uv[1] = uvs[idx * 2 + 1];
        } else {
            v.uv[0] = 0.f;
            v.uv[1] = 0.f;
        }

        if (hasNormals) {
            v.normal[0] = normals[idx * 3 + 0];
            v.normal[1] = normals[idx * 3 + 1];
            v.normal[2] = normals[idx * 3 + 2];
        } else {
            v.normal[0] = 0.f;
            v.normal[1] = 0.f;
            v.normal[2] = 1.f;
        }

        outVertices.push_back(v);
    }

    return !outVertices.empty();
}
