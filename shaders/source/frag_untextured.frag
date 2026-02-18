#version 450

layout(push_constant) uniform Push {
    mat4 proj;
    vec4 color;
} pc;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

void main() {
    /* Basic directional lighting matching textured shader, but no texture sampling. */
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    vec3 normal = normalize(inNormal);
    float NdotL = max(dot(normal, lightDir), 0.0);
    float ambient = 0.3;
    float lighting = ambient + (1.0 - ambient) * NdotL;

    outColor = vec4(pc.color.rgb * lighting, pc.color.a);
}
