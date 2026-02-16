#pragma once

#include <memory>
#include <string>
#include <vector>

class MeshManager;
class MeshHandle;

/**
 * ProceduralMeshFactory: Generate basic geometric primitives with full vertex data (pos+UV+normal).
 * All meshes use the VertexData struct (interleaved pos+UV+normal, 32 bytes/vertex).
 */
namespace ProceduralMeshFactory {

/**
 * Create a procedural mesh from a string identifier.
 * Supported types: "cube", "triangle", "rectangle", "sphere", "cylinder", "cone"
 * Returns nullptr if type is not recognized.
 */
std::shared_ptr<MeshHandle> CreateMesh(const std::string& type, MeshManager* pMeshManager);

/**
 * Individual mesh generators (called by CreateMesh).
 */
std::shared_ptr<MeshHandle> CreateCube(MeshManager* pMeshManager);
std::shared_ptr<MeshHandle> CreateTriangle(MeshManager* pMeshManager);
std::shared_ptr<MeshHandle> CreateRectangle(MeshManager* pMeshManager);
std::shared_ptr<MeshHandle> CreateSphere(MeshManager* pMeshManager, int segments = 32);
std::shared_ptr<MeshHandle> CreateCylinder(MeshManager* pMeshManager, int segments = 32);
std::shared_ptr<MeshHandle> CreateCone(MeshManager* pMeshManager, int segments = 32);

} // namespace ProceduralMeshFactory
