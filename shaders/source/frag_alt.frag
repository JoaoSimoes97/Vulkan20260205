#version 450

/* Same push layout as main frag so pipelines are layout-compatible. Output: grayscale of color (tests different shader path). */
layout(push_constant) uniform Push {
    mat4 proj;
    vec4 color;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    float g = dot(pc.color.rgb, vec3(0.299, 0.587, 0.114));
    outColor = vec4(g, g, g, pc.color.a);
}
