#version 450

layout(push_constant) uniform Push {
    mat4 proj;
    vec4 color;
} pc;

layout(set = 0, binding = 0) uniform sampler2D uTex;

layout(location = 0) out vec4 outColor;

void main() {
    /* Sample texture (default white 1x1 when no UVs); tint by push constant color. */
    vec4 texColor = texture(uTex, vec2(0.5, 0.5));
    outColor = texColor * pc.color;
}
