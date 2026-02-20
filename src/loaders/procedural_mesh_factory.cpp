#include "procedural_mesh_factory.h"
#include "managers/mesh_manager.h"
#include "gltf_mesh_utils.h"
#include "vulkan/vulkan_utils.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace ProceduralMeshFactory {

std::shared_ptr<MeshHandle> CreateMesh(const std::string& type, MeshManager* pMeshManager) {
    if (type == "cube") {
        return CreateCube(pMeshManager);
    } else if (type == "triangle") {
        return CreateTriangle(pMeshManager);
    } else if (type == "rectangle") {
        return CreateRectangle(pMeshManager);
    } else if (type == "sphere") {
        return CreateSphere(pMeshManager);
    } else if (type == "cylinder") {
        return CreateCylinder(pMeshManager);
    } else if (type == "cone") {
        return CreateCone(pMeshManager);
    }
    VulkanUtils::LogErr("ProceduralMeshFactory: unknown type '{}'", type);
    return nullptr;
}

std::shared_ptr<MeshHandle> CreateCube(MeshManager* pMeshManager) {
    // Unit cube (-0.5 to +0.5), 24 vertices (4 per face for proper normals/UVs), 36 indices
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;

    // Helper lambda to add a quad (4 vertices, 6 indices for 2 triangles)
    auto addQuad = [&](const float pos[4][3], const float normal[3], const float uvs[4][2]) {
        uint32_t baseIdx = static_cast<uint32_t>(vertices.size());
        for (int i = 0; i < 4; ++i) {
            VertexData v;
            v.position[0] = pos[i][0]; v.position[1] = pos[i][1]; v.position[2] = pos[i][2];
            v.uv[0] = uvs[i][0]; v.uv[1] = uvs[i][1];
            v.normal[0] = normal[0]; v.normal[1] = normal[1]; v.normal[2] = normal[2];
            vertices.push_back(v);
        }
        // Two triangles: 0-1-2, 0-2-3
        indices.push_back(baseIdx + 0);
        indices.push_back(baseIdx + 1);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 0);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 3);
    };

    // Front face (+Z)
    float posFront[4][3] = {{-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}};
    float normalFront[3] = {0.0f, 0.0f, 1.0f};
    float uvsFront[4][2] = {{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}};
    addQuad(posFront, normalFront, uvsFront);

    // Back face (-Z)
    float posBack[4][3] = {{0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}};
    float normalBack[3] = {0.0f, 0.0f, -1.0f};
    float uvsBack[4][2] = {{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}};
    addQuad(posBack, normalBack, uvsBack);

    // Right face (+X)
    float posRight[4][3] = {{0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
    float normalRight[3] = {1.0f, 0.0f, 0.0f};
    float uvsRight[4][2] = {{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}};
    addQuad(posRight, normalRight, uvsRight);

    // Left face (-X)
    float posLeft[4][3] = {{-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f}};
    float normalLeft[3] = {-1.0f, 0.0f, 0.0f};
    float uvsLeft[4][2] = {{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}};
    addQuad(posLeft, normalLeft, uvsLeft);

    // Top face (+Y)
    float posTop[4][3] = {{-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}};
    float normalTop[3] = {0.0f, 1.0f, 0.0f};
    float uvsTop[4][2] = {{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}};
    addQuad(posTop, normalTop, uvsTop);

    // Bottom face (-Y)
    float posBottom[4][3] = {{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}};
    float normalBottom[3] = {0.0f, -1.0f, 0.0f};
    float uvsBottom[4][2] = {{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}};
    addQuad(posBottom, normalBottom, uvsBottom);

    // Convert indexed vertices to triangle list (expand indices)
    std::vector<VertexData> triangleVertices;
    triangleVertices.reserve(indices.size());
    for (uint32_t idx : indices) {
        triangleVertices.push_back(vertices[idx]);
    }

    const uint32_t vertexCount = static_cast<uint32_t>(triangleVertices.size());
    return pMeshManager->GetOrCreateFromGltf("procedural_cube", triangleVertices.data(), vertexCount);
}

std::shared_ptr<MeshHandle> CreateTriangle(MeshManager* pMeshManager) {
    // Equilateral triangle in XY plane, centered at origin, size 1.0
    const float height = 0.866f; // sqrt(3)/2
    std::vector<VertexData> vertices(3);
    
    // Bottom left
    vertices[0].position[0] = -0.5f; vertices[0].position[1] = -height/2.0f; vertices[0].position[2] = 0.0f;
    vertices[0].uv[0] = 0.0f; vertices[0].uv[1] = 1.0f;
    vertices[0].normal[0] = 0.0f; vertices[0].normal[1] = 0.0f; vertices[0].normal[2] = 1.0f;
    
    // Bottom right
    vertices[1].position[0] = 0.5f; vertices[1].position[1] = -height/2.0f; vertices[1].position[2] = 0.0f;
    vertices[1].uv[0] = 1.0f; vertices[1].uv[1] = 1.0f;
    vertices[1].normal[0] = 0.0f; vertices[1].normal[1] = 0.0f; vertices[1].normal[2] = 1.0f;
    
    // Top
    vertices[2].position[0] = 0.0f; vertices[2].position[1] = height/2.0f; vertices[2].position[2] = 0.0f;
    vertices[2].uv[0] = 0.5f; vertices[2].uv[1] = 0.0f;
    vertices[2].normal[0] = 0.0f; vertices[2].normal[1] = 0.0f; vertices[2].normal[2] = 1.0f;

    std::vector<uint32_t> indices = {0, 1, 2};
    const uint32_t vertexCount = static_cast<uint32_t>(vertices.size());
    return pMeshManager->GetOrCreateFromGltf("procedural_triangle", vertices.data(), vertexCount);
}

std::shared_ptr<MeshHandle> CreateRectangle(MeshManager* pMeshManager) {
    // Unit rectangle (quad) in XY plane, -0.5 to +0.5 in X and Y
    std::vector<VertexData> vertices(4);
    
    // Bottom-left
    vertices[0].position[0] = -0.5f; vertices[0].position[1] = -0.5f; vertices[0].position[2] = 0.0f;
    vertices[0].uv[0] = 0.0f; vertices[0].uv[1] = 1.0f;
    vertices[0].normal[0] = 0.0f; vertices[0].normal[1] = 0.0f; vertices[0].normal[2] = 1.0f;
    
    // Bottom-right
    vertices[1].position[0] = 0.5f; vertices[1].position[1] = -0.5f; vertices[1].position[2] = 0.0f;
    vertices[1].uv[0] = 1.0f; vertices[1].uv[1] = 1.0f;
    vertices[1].normal[0] = 0.0f; vertices[1].normal[1] = 0.0f; vertices[1].normal[2] = 1.0f;
    
    // Top-right
    vertices[2].position[0] = 0.5f; vertices[2].position[1] = 0.5f; vertices[2].position[2] = 0.0f;
    vertices[2].uv[0] = 1.0f; vertices[2].uv[1] = 0.0f;
    vertices[2].normal[0] = 0.0f; vertices[2].normal[1] = 0.0f; vertices[2].normal[2] = 1.0f;
    
    // Top-left
    vertices[3].position[0] = -0.5f; vertices[3].position[1] = 0.5f; vertices[3].position[2] = 0.0f;
    vertices[3].uv[0] = 0.0f; vertices[3].uv[1] = 0.0f;
    vertices[3].normal[0] = 0.0f; vertices[3].normal[1] = 0.0f; vertices[3].normal[2] = 1.0f;

    std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};
    
    // Convert indexed vertices to triangle list (expand indices)
    std::vector<VertexData> triangleVertices;
    triangleVertices.reserve(indices.size());
    for (uint32_t idx : indices) {
        triangleVertices.push_back(vertices[idx]);
    }
    
    const uint32_t vertexCount = static_cast<uint32_t>(triangleVertices.size());
    return pMeshManager->GetOrCreateFromGltf("procedural_rectangle", triangleVertices.data(), vertexCount);
}

std::shared_ptr<MeshHandle> CreateSphere(MeshManager* pMeshManager, int segments) {
    // UV sphere with radius 0.5
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    
    const float radius = 0.5f;
    const int rings = segments / 2;
    const int sectors = segments;
    
    // Generate vertices
    for (int r = 0; r <= rings; ++r) {
        float phi = static_cast<float>(r) / rings * glm::pi<float>();
        for (int s = 0; s <= sectors; ++s) {
            float theta = static_cast<float>(s) / sectors * 2.0f * glm::pi<float>();
            
            VertexData v;
            v.position[0] = radius * std::sin(phi) * std::cos(theta);
            v.position[1] = radius * std::cos(phi);
            v.position[2] = radius * std::sin(phi) * std::sin(theta);
            
            // Normal is normalized position for sphere
            v.normal[0] = v.position[0] / radius;
            v.normal[1] = v.position[1] / radius;
            v.normal[2] = v.position[2] / radius;
            
            v.uv[0] = static_cast<float>(s) / sectors;
            v.uv[1] = static_cast<float>(r) / rings;
            
            vertices.push_back(v);
        }
    }
    
    // Generate indices (CCW winding for outward-facing normals)
    // Quad layout viewed from outside:
    //   v0 --- v2
    //   |       |
    //   v1 --- v3
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            uint32_t v0 = r * (sectors + 1) + s;
            uint32_t v1 = v0 + sectors + 1;  // below v0
            uint32_t v2 = v0 + 1;             // right of v0
            uint32_t v3 = v1 + 1;             // below v2
            
            // Two triangles per quad (CCW from outside)
            indices.push_back(v0);
            indices.push_back(v2);
            indices.push_back(v1);
            
            indices.push_back(v2);
            indices.push_back(v3);
            indices.push_back(v1);
        }
    }
    
    // Convert indexed vertices to triangle list (expand indices)
    std::vector<VertexData> triangleVertices;
    triangleVertices.reserve(indices.size());
    for (uint32_t idx : indices) {
        triangleVertices.push_back(vertices[idx]);
    }
    
    const uint32_t vertexCount = static_cast<uint32_t>(triangleVertices.size());
    return pMeshManager->GetOrCreateFromGltf("procedural_sphere", triangleVertices.data(), vertexCount);
}

std::shared_ptr<MeshHandle> CreateCylinder(MeshManager* pMeshManager, int segments) {
    // Unit cylinder (radius 0.5, height 1.0, centered at origin)
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    
    const float radius = 0.5f;
    const float halfHeight = 0.5f;
    
    // Side vertices (duplicate for proper normals)
    for (int s = 0; s <= segments; ++s) {
        float theta = static_cast<float>(s) / segments * 2.0f * glm::pi<float>();
        float x = radius * std::cos(theta);
        float z = radius * std::sin(theta);
        
        // Bottom vertex
        VertexData vBottom;
        vBottom.position[0] = x; vBottom.position[1] = -halfHeight; vBottom.position[2] = z;
        vBottom.normal[0] = std::cos(theta); vBottom.normal[1] = 0.0f; vBottom.normal[2] = std::sin(theta);
        vBottom.uv[0] = static_cast<float>(s) / segments; vBottom.uv[1] = 1.0f;
        vertices.push_back(vBottom);
        
        // Top vertex
        VertexData vTop;
        vTop.position[0] = x; vTop.position[1] = halfHeight; vTop.position[2] = z;
        vTop.normal[0] = std::cos(theta); vTop.normal[1] = 0.0f; vTop.normal[2] = std::sin(theta);
        vTop.uv[0] = static_cast<float>(s) / segments; vTop.uv[1] = 0.0f;
        vertices.push_back(vTop);
    }
    
    // Side indices (CCW winding for outward-facing normals)
    // Quad layout viewed from outside:
    //   v1 (top) --- v3 (top)
    //   |              |
    //   v0 (bot) --- v2 (bot)
    for (int s = 0; s < segments; ++s) {
        uint32_t v0 = s * 2;       // bottom at angle s
        uint32_t v1 = v0 + 1;      // top at angle s
        uint32_t v2 = v0 + 2;      // bottom at angle s+1
        uint32_t v3 = v0 + 3;      // top at angle s+1
        
        // CCW from outside: BL→TL→BR, TL→TR→BR
        indices.push_back(v0); indices.push_back(v1); indices.push_back(v2);
        indices.push_back(v1); indices.push_back(v3); indices.push_back(v2);
    }
    
    // Convert indexed vertices to triangle list (expand indices)
    std::vector<VertexData> triangleVertices;
    triangleVertices.reserve(indices.size());
    for (uint32_t idx : indices) {
        triangleVertices.push_back(vertices[idx]);
    }
    
    // TODO: Add caps (top and bottom circles) for completeness
    
    const uint32_t vertexCount = static_cast<uint32_t>(triangleVertices.size());
    return pMeshManager->GetOrCreateFromGltf("procedural_cylinder", triangleVertices.data(), vertexCount);
}

std::shared_ptr<MeshHandle> CreateCone(MeshManager* pMeshManager, int segments) {
    // Unit cone (base radius 0.5, height 1.0, apex at +Y)
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    
    const float radius = 0.5f;
    const float height = 1.0f;
    const float halfHeight = 0.5f;
    
    // Apex vertex
    VertexData apex;
    apex.position[0] = 0.0f; apex.position[1] = halfHeight; apex.position[2] = 0.0f;
    apex.normal[0] = 0.0f; apex.normal[1] = 1.0f; apex.normal[2] = 0.0f;
    apex.uv[0] = 0.5f; apex.uv[1] = 0.0f;
    vertices.push_back(apex);
    
    // Base vertices
    for (int s = 0; s <= segments; ++s) {
        float theta = static_cast<float>(s) / segments * 2.0f * glm::pi<float>();
        float x = radius * std::cos(theta);
        float z = radius * std::sin(theta);
        
        VertexData v;
        v.position[0] = x; v.position[1] = -halfHeight; v.position[2] = z;
        
        // Approximate normal for cone side
        float nx = std::cos(theta);
        float ny = radius / height;
        float nz = std::sin(theta);
        float len = std::sqrt(nx*nx + ny*ny + nz*nz);
        v.normal[0] = nx / len; v.normal[1] = ny / len; v.normal[2] = nz / len;
        
        v.uv[0] = static_cast<float>(s) / segments; v.uv[1] = 1.0f;
        vertices.push_back(v);
    }
    
    // Side indices (triangles from apex to base)
    for (int s = 0; s < segments; ++s) {
        indices.push_back(0);
        indices.push_back(s + 1);
        indices.push_back(s + 2);
    }
    
    // Convert indexed vertices to triangle list (expand indices)
    std::vector<VertexData> triangleVertices;
    triangleVertices.reserve(indices.size());
    for (uint32_t idx : indices) {
        triangleVertices.push_back(vertices[idx]);
    }
    
    // TODO: Add base circle for completeness
    
    const uint32_t vertexCount = static_cast<uint32_t>(triangleVertices.size());
    return pMeshManager->GetOrCreateFromGltf("procedural_cone", triangleVertices.data(), vertexCount);
}

} // namespace ProceduralMeshFactory
