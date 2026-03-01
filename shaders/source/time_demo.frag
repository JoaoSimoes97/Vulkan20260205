#version 450

/* Time-demo fragment shader: color driven by globalUBO.time (pulse). */

layout(std140, set = 0, binding = 1) uniform GlobalUBO {
    float time;
    float deltaTime;
    float _pad0;
    float _pad1;
} globalUBO;

layout(location = 0) out vec4 outColor;

void main() {
    float t = globalUBO.time;
    float r = sin(t) * 0.5 + 0.5;
    float g = cos(t * 0.7) * 0.5 + 0.5;
    float b = sin(t * 1.3) * 0.5 + 0.5;
    outColor = vec4(r, g, b, 1.0);
}
