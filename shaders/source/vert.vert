#version 450

layout(push_constant) uniform Push {
    mat4 proj;
    vec4 color;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;

void main() {
    gl_Position = pc.proj * vec4(inPosition, 1.0);
    outUV = inUV;
    outNormal = inNormal; // For now, no model transform (or assume orthonormal)
}
