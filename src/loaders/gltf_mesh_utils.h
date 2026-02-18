#pragma once

#include <cstdint>
#include <vector>
#include <tiny_gltf.h>

/**
 * Vertex layout: interleaved position + UV + normal (8 floats per vertex).
 * Stride = 32 bytes. Offsets: position 0, UV 12, normal 20.
 */
struct VertexData {
    float position[3];  // offset 0
    float uv[2];        // offset 12
    float normal[3];    // offset 20
    // Total: 32 bytes per vertex
};

constexpr uint32_t kVertexStride = sizeof(VertexData);

/**
 * Extract vertex data (position + UV + normal) from a glTF mesh for upload to GPU.
 * Expands indexed primitives to non-indexed (so engine can use vkCmdDraw without index buffer).
 * Returns true and fills outVertices on success. Missing UVs default to (0,0); missing normals default to (0,0,1).
 */
bool GetMeshDataFromGltf(const tinygltf::Model& model, int meshIndex, int primitiveIndex,
                         std::vector<VertexData>& outVertices);
