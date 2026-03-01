#version 450

/* Time-demo vertex shader: draws a cube with viewProj + model from push constants, GlobalUBO at binding 1. */

layout(push_constant) uniform Push {
    mat4 viewProj;
    mat4 model;
} pc;

layout(std140, set = 0, binding = 1) uniform GlobalUBO {
    float time;
    float deltaTime;
    float _pad0;
    float _pad1;
} globalUBO;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;

void main() {
    /* Apply time-based scale pulse (object transform from level + pulse) */
    float scale = 1.0 + 0.2 * sin(globalUBO.time);
    vec4 worldPos = pc.model * vec4(inPosition * scale, 1.0);
    gl_Position = pc.viewProj * worldPos;
}
