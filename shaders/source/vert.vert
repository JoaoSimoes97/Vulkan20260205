#version 450

/* ---- Push Constants (Instanced Rendering) ---- */
/* 96 bytes total - shared per draw call batch */
layout(push_constant) uniform Push {
    mat4 viewProj;        // View-Projection matrix (64 bytes)
    vec4 camPos;          // Camera world position xyz, w unused (16 bytes)
    uint batchStartIndex; // Start index into SSBO for this batch (4 bytes)
    uint _pad0;           // Padding (4 bytes)
    uint _pad1;           // Padding (4 bytes)
    uint _pad2;           // Padding (4 bytes)
    // Total: 96 bytes
} pc;

/* ---- Object Data SSBO (binding 2) ---- */
/* Must match C++ ObjectData = 256 bytes */
struct ObjectData {
    mat4 model;      // Model matrix (64 bytes)
    vec4 emissive;   // Emissive RGB + strength (16 bytes)
    vec4 matProps;   // x=metallic, y=roughness, z=normalScale, w=occlusion (16 bytes)
    vec4 baseColor;  // Base color RGBA (16 bytes)
    vec4 reserved0;  // 16 bytes
    vec4 reserved1;  // 16 bytes
    vec4 reserved2;  // 16 bytes
    vec4 reserved3;  // 16 bytes
    vec4 reserved4;  // 16 bytes
    vec4 reserved5;  // 16 bytes
    vec4 reserved6;  // 16 bytes
    vec4 reserved7;  // 16 bytes
    vec4 reserved8;  // 16 bytes
    // Total: 64 + 12*16 = 256 bytes
};

layout(std430, set = 0, binding = 2) readonly buffer ObjectDataBlock {
    ObjectData objects[];
} objectData;

/* ---- Vertex Inputs ---- */
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;

/* ---- Outputs to Fragment Shader ---- */
layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outWorldPos;
layout(location = 3) flat out uint outObjectIndex;

void main() {
    // Calculate object index using instancing: batch start + instance ID
    uint objIdx = pc.batchStartIndex + gl_InstanceIndex;
    
    // Get model matrix from SSBO
    mat4 model = objectData.objects[objIdx].model;
    
    // Transform to clip space: compute MVP in shader using viewProj and model
    gl_Position = pc.viewProj * model * vec4(inPosition, 1.0);
    
    // Pass through UV (with wrapping handled in fragment shader)
    outUV = inUV;
    
    // Transform normal to world space
    // Using mat3(model) for non-uniform scale; transpose(inverse()) for correct normals
    mat3 normalMatrix = mat3(model);
    outNormal = normalize(normalMatrix * inNormal);
    
    // World position for lighting calculations
    outWorldPos = (model * vec4(inPosition, 1.0)).xyz;
    
    // Pass object index to fragment shader for material lookup
    outObjectIndex = objIdx;
}
