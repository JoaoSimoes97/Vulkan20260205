/*
 * Extract vertex data (position + UV + normal) from tinygltf meshes for engine upload.
 */
#include "gltf_mesh_utils.h"
#include <tiny_gltf.h>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <limits>

namespace {

size_t ComponentSizeBytes(int componentType) {
    switch (componentType) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        return 1u;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        return 2u;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        return 4u;
    default:
        return 0u;
    }
}

size_t TypeComponentCount(int type) {
    switch (type) {
    case TINYGLTF_TYPE_SCALAR: return 1u;
    case TINYGLTF_TYPE_VEC2: return 2u;
    case TINYGLTF_TYPE_VEC3: return 3u;
    case TINYGLTF_TYPE_VEC4: return 4u;
    default: return 0u;
    }
}

float ReadComponentAsFloat(const unsigned char* p, int componentType, bool normalized) {
    switch (componentType) {
    case TINYGLTF_COMPONENT_TYPE_FLOAT: {
        float v = 0.f;
        std::memcpy(&v, p, sizeof(float));
        return v;
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
        const uint8_t v = *reinterpret_cast<const uint8_t*>(p);
        return normalized ? (static_cast<float>(v) / 255.0f) : static_cast<float>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_BYTE: {
        const int8_t v = *reinterpret_cast<const int8_t*>(p);
        if (!normalized) return static_cast<float>(v);
        return std::max(-1.0f, static_cast<float>(v) / 127.0f);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        uint16_t v = 0;
        std::memcpy(&v, p, sizeof(uint16_t));
        return normalized ? (static_cast<float>(v) / 65535.0f) : static_cast<float>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_SHORT: {
        int16_t v = 0;
        std::memcpy(&v, p, sizeof(int16_t));
        if (!normalized) return static_cast<float>(v);
        return std::max(-1.0f, static_cast<float>(v) / 32767.0f);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
        uint32_t v = 0;
        std::memcpy(&v, p, sizeof(uint32_t));
        if (!normalized) return static_cast<float>(v);
        return static_cast<float>(static_cast<double>(v) / 4294967295.0);
    }
    default:
        return 0.f;
    }
}

bool ReadAccessorAsFloatN(const tinygltf::Model& model, int accessorIndex, int expectedType, std::vector<float>& outData) {
    outData.clear();
    if (accessorIndex < 0 || size_t(accessorIndex) >= model.accessors.size())
        return false;
    const tinygltf::Accessor& acc = model.accessors[size_t(accessorIndex)];
    if (acc.type != expectedType)
        return false;
    if (acc.bufferView < 0 || size_t(acc.bufferView) >= model.bufferViews.size())
        return false;

    const tinygltf::BufferView& bv = model.bufferViews[size_t(acc.bufferView)];
    if (bv.buffer < 0 || size_t(bv.buffer) >= model.buffers.size())
        return false;
    const tinygltf::Buffer& buf = model.buffers[size_t(bv.buffer)];

    const size_t compCount = TypeComponentCount(acc.type);
    const size_t compSize = ComponentSizeBytes(acc.componentType);
    if (compCount == 0u || compSize == 0u)
        return false;

    const size_t elementSize = compCount * compSize;
    const size_t stride = (bv.byteStride > 0) ? size_t(bv.byteStride) : elementSize;
    const size_t baseOffset = size_t(bv.byteOffset) + size_t(acc.byteOffset);
    if (baseOffset + size_t(acc.count) * stride > buf.data.size())
        return false;

    outData.resize(size_t(acc.count) * compCount);
    const unsigned char* src = buf.data.data() + baseOffset;
    for (size_t i = 0; i < size_t(acc.count); ++i) {
        const unsigned char* elem = src + i * stride;
        for (size_t c = 0; c < compCount; ++c) {
            outData[i * compCount + c] = ReadComponentAsFloat(elem + c * compSize, acc.componentType, acc.normalized);
        }
    }
    return true;
}

bool ReadIndexAccessorAsU32(const tinygltf::Model& model, int accessorIndex, std::vector<uint32_t>& outIndices) {
    outIndices.clear();
    if (accessorIndex < 0 || size_t(accessorIndex) >= model.accessors.size())
        return false;
    const tinygltf::Accessor& acc = model.accessors[size_t(accessorIndex)];
    if (acc.type != TINYGLTF_TYPE_SCALAR)
        return false;
    if (acc.bufferView < 0 || size_t(acc.bufferView) >= model.bufferViews.size())
        return false;

    const tinygltf::BufferView& bv = model.bufferViews[size_t(acc.bufferView)];
    if (bv.buffer < 0 || size_t(bv.buffer) >= model.buffers.size())
        return false;
    const tinygltf::Buffer& buf = model.buffers[size_t(bv.buffer)];

    const size_t compSize = ComponentSizeBytes(acc.componentType);
    if (compSize == 0u)
        return false;
    const size_t stride = (bv.byteStride > 0) ? size_t(bv.byteStride) : compSize;
    const size_t baseOffset = size_t(bv.byteOffset) + size_t(acc.byteOffset);
    if (baseOffset + size_t(acc.count) * stride > buf.data.size())
        return false;

    outIndices.resize(size_t(acc.count));
    const unsigned char* src = buf.data.data() + baseOffset;
    for (size_t i = 0; i < size_t(acc.count); ++i) {
        const unsigned char* p = src + i * stride;
        switch (acc.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            outIndices[i] = static_cast<uint32_t>(*reinterpret_cast<const uint8_t*>(p));
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            uint16_t v = 0;
            std::memcpy(&v, p, sizeof(uint16_t));
            outIndices[i] = static_cast<uint32_t>(v);
            break;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            uint32_t v = 0;
            std::memcpy(&v, p, sizeof(uint32_t));
            outIndices[i] = v;
            break;
        }
        default:
            return false;
        }
    }
    return true;
}

bool BuildTriangleListIndices(const tinygltf::Primitive& prim,
                              const std::vector<uint32_t>& sourceIndices,
                              std::vector<uint32_t>& outTriangleIndices) {
    outTriangleIndices.clear();
    const int mode = prim.mode;
    if (mode == TINYGLTF_MODE_TRIANGLES) {
        if (sourceIndices.size() < 3u)
            return false;
        outTriangleIndices = sourceIndices;
        return true;
    }
    if (mode == TINYGLTF_MODE_TRIANGLE_STRIP) {
        if (sourceIndices.size() < 3u)
            return false;
        outTriangleIndices.reserve((sourceIndices.size() - 2u) * 3u);
        for (size_t i = 2; i < sourceIndices.size(); ++i) {
            const uint32_t a = sourceIndices[i - 2];
            const uint32_t b = sourceIndices[i - 1];
            const uint32_t c = sourceIndices[i];
            if (i % 2u == 0u) {
                outTriangleIndices.push_back(a);
                outTriangleIndices.push_back(b);
                outTriangleIndices.push_back(c);
            } else {
                outTriangleIndices.push_back(b);
                outTriangleIndices.push_back(a);
                outTriangleIndices.push_back(c);
            }
        }
        return !outTriangleIndices.empty();
    }
    if (mode == TINYGLTF_MODE_TRIANGLE_FAN) {
        if (sourceIndices.size() < 3u)
            return false;
        outTriangleIndices.reserve((sourceIndices.size() - 2u) * 3u);
        const uint32_t root = sourceIndices[0];
        for (size_t i = 2; i < sourceIndices.size(); ++i) {
            outTriangleIndices.push_back(root);
            outTriangleIndices.push_back(sourceIndices[i - 1]);
            outTriangleIndices.push_back(sourceIndices[i]);
        }
        return !outTriangleIndices.empty();
    }
    return false;
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
    if (!ReadAccessorAsFloatN(model, itPos->second, TINYGLTF_TYPE_VEC3, positions)) {
        std::cerr << "[GetMeshDataFromGltf] Failed to read POSITION data.\n";
        return false;
    }
    const size_t vertexCount = positions.size() / 3u;
    if (vertexCount == 0)
        return false;

    // TEXCOORD_0 (UV) is optional, default to (0,0)
    std::vector<float> uvs;
    auto itUV = prim.attributes.find("TEXCOORD_0");
    if (itUV != prim.attributes.end() && ReadAccessorAsFloatN(model, itUV->second, TINYGLTF_TYPE_VEC2, uvs)) {
        if (uvs.size() / 2u != vertexCount) {
            std::cerr << "[GetMeshDataFromGltf] UV count mismatch, ignoring UVs.\n";
            uvs.clear();
        }
    }
    const bool hasUVs = !uvs.empty();

    // NORMAL is optional, default to (0,0,1)
    std::vector<float> normals;
    auto itNorm = prim.attributes.find("NORMAL");
    if (itNorm != prim.attributes.end() && ReadAccessorAsFloatN(model, itNorm->second, TINYGLTF_TYPE_VEC3, normals)) {
        if (normals.size() / 3u != vertexCount) {
            std::cerr << "[GetMeshDataFromGltf] Normal count mismatch, ignoring normals.\n";
            normals.clear();
        }
    }
    const bool hasNormals = !normals.empty();

    // Read source indices (if any)
    std::vector<uint32_t> sourceIndices;
    const bool indexed = (prim.indices >= 0);
    if (indexed) {
        if (!ReadIndexAccessorAsU32(model, prim.indices, sourceIndices)) {
            std::cerr << "[GetMeshDataFromGltf] Failed to read index data.\n";
            return false;
        }
    } else {
        // Non-indexed: generate trivial indices
        sourceIndices.resize(vertexCount);
        for (size_t i = 0; i < vertexCount; ++i)
            sourceIndices[i] = static_cast<uint32_t>(i);
    }

    // Build triangle index list from primitive mode
    std::vector<uint32_t> indices;
    if (!BuildTriangleListIndices(prim, sourceIndices, indices)) {
        std::cerr << "[GetMeshDataFromGltf] Unsupported or invalid primitive mode " << prim.mode << ".\n";
        return false;
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
