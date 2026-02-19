#version 450

/* Alternative/debug shader - grayscale output for testing.
   Layout-compatible with main shaders. */

/* ---- Push Constants ---- */
layout(push_constant) uniform Push {
    mat4 mvp;
    vec4 color;
    uint objectIndex;
} pc;

/* ---- Object Data SSBO (binding 2) for layout compatibility ---- */
struct ObjectData {
    mat4 model;
    vec4 emissive;
    vec4 matProps;
    vec4 baseColor;
    vec4 reserved0;
    vec4 reserved1;
    vec4 reserved2;
    vec4 reserved3;
    vec4 reserved4;
    vec4 reserved5;
    vec4 reserved6;
    vec4 reserved7;
    vec4 reserved8;
};

layout(std430, set = 0, binding = 2) readonly buffer ObjectDataBlock {
    ObjectData objects[];
} objectData;

/* ---- Light Data SSBO (binding 3) for layout compatibility ---- */
struct GpuLight {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 params;
};

layout(std430, set = 0, binding = 3) readonly buffer LightDataBlock {
    uint lightCount;
    uint _pad1;
    uint _pad2;
    uint _pad3;
    GpuLight lights[];
} lightData;

/* ---- Inputs from Vertex Shader ---- */
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inWorldPos;
layout(location = 3) flat in uint inObjectIndex;

/* ---- Output ---- */
layout(location = 0) out vec4 outColor;

void main() {
    // Simple grayscale from color for testing/debugging
    float luminance = dot(pc.color.rgb, vec3(0.299, 0.587, 0.114));
    outColor = vec4(luminance, luminance, luminance, pc.color.a);
}
