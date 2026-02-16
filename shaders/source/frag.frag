#version 450

layout(push_constant) uniform Push {
    mat4 proj;
    vec4 color;
} pc;

layout(set = 0, binding = 0) uniform sampler2D uTex;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

void main() {
    /* Sample texture using actual UVs; tint by push constant color. */
    vec4 texColor = texture(uTex, inUV);
    
    /* Basic directional lighting: light from above-right (0.5, 1.0, 0.3). */
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    vec3 normal = normalize(inNormal);
    float NdotL = max(dot(normal, lightDir), 0.0);
    float ambient = 0.3;
    float lighting = ambient + (1.0 - ambient) * NdotL;
    
    outColor = texColor * pc.color * lighting;
}
