#pragma once

#include <vector>

namespace tinygltf {
struct Model;
}

/**
 * Extract vertex position data (xyz per vertex) from a glTF mesh for upload to GPU.
 * Expands indexed primitives to non-indexed (so engine can use vkCmdDraw without index buffer).
 * Returns true and fills outPositions (size = vertexCount * 3) on success.
 */
bool GetMeshPositionsFromGltf(const tinygltf::Model& model, int meshIndex, int primitiveIndex,
                               std::vector<float>& outPositions);
