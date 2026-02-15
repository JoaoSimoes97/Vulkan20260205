#version 450

layout(push_constant) uniform Push {
    mat4 proj;
    vec4 color;
} pc;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = pc.proj * vec4(inPosition, 1.0);
}
