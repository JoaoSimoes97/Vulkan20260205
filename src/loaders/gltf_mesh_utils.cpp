/*
 * Extract vertex position data from tinygltf meshes for engine upload.
 */
#include "gltf_mesh_utils.h"
#include <tiny_gltf.h>
#include <cstring>

namespace {

bool GetAccessorDataAsFloat3(const tinygltf::Model& model, int accessorIndex,
                             std::vector<float>& outPositions, size_t* outStrideBytes) {
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
    outPositions.resize(size_t(acc.count) * numComp);
    const unsigned char* src = buf.data.data() + offset;
    for (size_t i = 0; i < size_t(acc.count); ++i) {
        std::memcpy(&outPositions[i * numComp], src + i * stride, compSize * numComp);
    }
    if (outStrideBytes)
        *outStrideBytes = stride;
    return true;
}

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

bool GetMeshPositionsFromGltf(const tinygltf::Model& model, int meshIndex, int primitiveIndex,
                              std::vector<float>& outPositions) {
    outPositions.clear();
    if (meshIndex < 0 || size_t(meshIndex) >= model.meshes.size())
        return false;
    const tinygltf::Mesh& mesh = model.meshes[size_t(meshIndex)];
    if (primitiveIndex < 0 || size_t(primitiveIndex) >= mesh.primitives.size())
        return false;
    const tinygltf::Primitive& prim = mesh.primitives[size_t(primitiveIndex)];
    auto it = prim.attributes.find("POSITION");
    if (it == prim.attributes.end())
        return false;
    std::vector<float> positions;
    if (!GetAccessorDataAsFloat3(model, it->second, positions, nullptr))
        return false;
    if (prim.indices >= 0) {
        std::vector<uint32_t> indices;
        if (!GetIndexDataAsU32(model, prim.indices, indices))
            return false;
        outPositions.clear();
        outPositions.reserve(indices.size() * 3u);
        for (uint32_t idx : indices) {
            if (size_t(idx) * 3 + 2 < positions.size()) {
                outPositions.push_back(positions[size_t(idx) * 3]);
                outPositions.push_back(positions[size_t(idx) * 3 + 1]);
                outPositions.push_back(positions[size_t(idx) * 3 + 2]);
            }
        }
    } else {
        outPositions = std::move(positions);
    }
    return !outPositions.empty();
}
