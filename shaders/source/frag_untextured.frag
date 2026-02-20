#version 450

/* ---- Push Constants ---- */
layout(push_constant) uniform Push {
    mat4 mvp;           // Model-View-Projection (unused in frag)
    vec4 color;         // Per-object tint color
    uint objectIndex;   // Index into object data SSBO
    uint _pad0;
    uint _pad1;
    uint _pad2;
    vec4 camPos;        // Camera world position
} pc;

/* ---- Object Data SSBO (binding 2) ---- */
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
};

layout(std430, set = 0, binding = 2) readonly buffer ObjectDataBlock {
    ObjectData objects[];
} objectData;

/* ---- Light Data SSBO (binding 3) ---- */
struct GpuLight {
    vec4 position;    // xyz = world position, w = range
    vec4 direction;   // xyz = direction (normalized), w = type (0=dir, 1=point, 2=spot)
    vec4 color;       // rgb = color, a = intensity
    vec4 params;      // x = innerCone, y = outerCone, z = falloff, w = active
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

/* ---- Constants ---- */
const float PI = 3.14159265359;
const float MIN_ROUGHNESS = 0.04;
const uint MAX_LIGHTS = 256u;

/* ---- PBR Functions ---- */

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float CalcAttenuation(float distance, float range, float falloff) {
    float d = distance / range;
    float attenuation = 1.0 / (1.0 + pow(d, falloff));
    float falloffSmooth = clamp(1.0 - pow(d, 4.0), 0.0, 1.0);
    return attenuation * falloffSmooth * falloffSmooth;
}

float CalcSpotAttenuation(vec3 L, vec3 spotDir, float innerCone, float outerCone) {
    float theta = dot(L, normalize(-spotDir));
    float epsilon = cos(innerCone) - cos(outerCone);
    return clamp((theta - cos(outerCone)) / max(epsilon, 0.0001), 0.0, 1.0);
}

void main() {
    // Get per-object data
    ObjectData objData = objectData.objects[inObjectIndex];
    
    // Material properties (no texture - use baseColor directly)
    vec3 albedo = pc.color.rgb * objData.baseColor.rgb;
    float metallic = clamp(objData.matProps.x, 0.0, 1.0);
    float roughness = clamp(objData.matProps.y, MIN_ROUGHNESS, 1.0);
    vec3 emissive = objData.emissive.rgb * objData.emissive.a;
    
    // Normal and view direction
    vec3 N = normalize(inNormal);
    vec3 camPos = vec3(0.0, 2.0, 8.0);
    vec3 V = normalize(camPos - inWorldPos);
    
    // Base reflectance (F0)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    // Accumulate lighting
    vec3 Lo = vec3(0.0);
    
    uint numLights = min(lightData.lightCount, MAX_LIGHTS);
    
    for (uint i = 0u; i < numLights; ++i) {
        GpuLight light = lightData.lights[i];
        
        if (light.params.w < 0.5)
            continue;
        
        vec3 L;
        float attenuation = 1.0;
        float lightType = light.direction.w;
        
        if (lightType < 0.5) {
            L = normalize(-light.direction.xyz);
        } else if (lightType < 1.5) {
            vec3 lightVec = light.position.xyz - inWorldPos;
            float distance = length(lightVec);
            L = normalize(lightVec);
            attenuation = CalcAttenuation(distance, light.position.w, light.params.z);
        } else {
            vec3 lightVec = light.position.xyz - inWorldPos;
            float distance = length(lightVec);
            L = normalize(lightVec);
            attenuation = CalcAttenuation(distance, light.position.w, light.params.z);
            attenuation *= CalcSpotAttenuation(L, light.direction.xyz, light.params.x, light.params.y);
        }
        
        vec3 H = normalize(V + L);
        vec3 radiance = light.color.rgb * light.color.a * attenuation;
        
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    // Default light if no lights
    if (numLights == 0u) {
        vec3 L = normalize(vec3(0.5, 1.0, 0.3));
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        vec3 diffuse = albedo * NdotL;
        float spec = pow(max(dot(N, H), 0.0), mix(8.0, 128.0, 1.0 - roughness));
        vec3 specular = vec3(spec) * mix(vec3(0.04), albedo, metallic);
        Lo = diffuse * 0.8 + specular * 0.3;
    }
    
    // Procedural sky environment for reflections (same as frag.frag)
    vec3 R = reflect(-V, N);
    float skyGradient = clamp(R.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 skyColor = mix(vec3(0.9, 0.85, 0.8), vec3(0.3, 0.5, 0.95), skyGradient);
    vec3 groundColor = vec3(0.25, 0.22, 0.18);
    vec3 envColor = R.y > 0.0 ? skyColor : groundColor;
    
    // Fresnel for environment reflection
    vec3 F_env = F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - max(dot(N, V), 0.0), 0.0, 1.0), 5.0);
    
    // Metals get stronger environment reflection
    float envStrength = mix(0.3, 1.5, metallic) * (1.0 - roughness * 0.7);
    vec3 envReflection = envColor * F_env * envStrength;
    
    vec3 kD_env = (1.0 - F_env) * (1.0 - metallic);
    vec3 ambientDiffuse = kD_env * albedo * vec3(0.15);
    
    vec3 ambient = ambientDiffuse + envReflection;
    vec3 color = ambient + Lo + emissive;
    
    // Tone mapping + gamma
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    
    outColor = vec4(color, pc.color.a * objData.baseColor.a);
}
