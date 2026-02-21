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

/* ---- Color Space Conversion ---- */
// sRGB to Linear (accurate piecewise, matches Khronos reference)
vec3 sRGBToLinear(vec3 srgb) {
    return mix(
        srgb / 12.92,
        pow((srgb + 0.055) / 1.055, vec3(2.4)),
        step(vec3(0.04045), srgb)
    );
}

// Linear to sRGB (accurate piecewise)
vec3 linearToSRGB(vec3 linear) {
    return mix(
        linear * 12.92,
        1.055 * pow(linear, vec3(1.0 / 2.4)) - 0.055,
        step(vec3(0.0031308), linear)
    );
}

// ACES Filmic Tone Mapping (better highlight roll-off than Reinhard)
vec3 toneMapACES(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

/* ---- PBR Functions ---- */

// Normal Distribution Function (GGX/Trowbridge-Reitz)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0001);
}

// Smith Joint GGX Visibility (Khronos reference, replaces GeometrySmith)
// Note: Vis = G / (4 * NdotL * NdotV)
float V_GGX(float NdotL, float NdotV, float alphaRoughness) {
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    
    float GGX = GGXV + GGXL;
    return GGX > 0.0 ? 0.5 / GGX : 0.0;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness (for IBL/environment reflections)
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Minimum distance to prevent numerical instability when light source is at surface
const float MIN_LIGHT_DISTANCE = 0.01;

float CalcAttenuation(float distance, float range, float falloff) {
    // Clamp distance to minimum to prevent division by near-zero
    float clampedDistance = max(distance, MIN_LIGHT_DISTANCE);
    
    // Inverse-square falloff (physically based)
    float distanceSquared = clampedDistance * clampedDistance;
    float rangeAttenuation = 1.0 / distanceSquared;
    
    // Smooth window function to avoid hard cutoff at range
    if (range > 0.0) {
        float d_over_r = clampedDistance / range;
        float d_over_r_4 = d_over_r * d_over_r * d_over_r * d_over_r;
        float smoothWindow = clamp(1.0 - d_over_r_4, 0.0, 1.0);
        rangeAttenuation *= smoothWindow * smoothWindow;
    }
    
    return rangeAttenuation;
}

float CalcSpotAttenuation(vec3 L, vec3 spotDir, float innerCone, float outerCone) {
    float theta = dot(L, normalize(-spotDir));
    float epsilon = cos(innerCone) - cos(outerCone);
    return clamp((theta - cos(outerCone)) / max(epsilon, 0.0001), 0.0, 1.0);
}

void main() {
    // Get per-object data
    ObjectData objData = objectData.objects[inObjectIndex];
    
    // Material properties (no texture - use baseColor directly, apply sRGB to linear)
    vec3 albedo = sRGBToLinear(pc.color.rgb * objData.baseColor.rgb);
    float metallic = clamp(objData.matProps.x, 0.0, 1.0);
    float roughness = clamp(objData.matProps.y, MIN_ROUGHNESS, 1.0);
    float alphaRoughness = roughness * roughness;
    vec3 emissive = sRGBToLinear(objData.emissive.rgb) * objData.emissive.a;
    
    // Normal and view direction (use pc.camPos from push constants)
    vec3 N = normalize(inNormal);
    vec3 V = normalize(pc.camPos.xyz - inWorldPos);
    
    // Base reflectance (F0)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    float NdotV = max(dot(N, V), 0.0);
    
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
            
            // Skip this light if too close (avoids NaN from normalizing near-zero vector)
            if (distance < MIN_LIGHT_DISTANCE) {
                continue;
            }
            
            L = lightVec / distance;
            attenuation = CalcAttenuation(distance, light.position.w, light.params.z);
        } else {
            vec3 lightVec = light.position.xyz - inWorldPos;
            float distance = length(lightVec);
            
            // Skip this light if too close (avoids NaN from normalizing near-zero vector)
            if (distance < MIN_LIGHT_DISTANCE) {
                continue;
            }
            
            L = lightVec / distance;
            attenuation = CalcAttenuation(distance, light.position.w, light.params.z);
            attenuation *= CalcSpotAttenuation(L, light.direction.xyz, light.params.x, light.params.y);
        }
        
        float NdotL = max(dot(N, L), 0.0);
        vec3 H = normalize(V + L);
        vec3 radiance = light.color.rgb * light.color.a * attenuation;
        
        // Cook-Torrance BRDF (using V_GGX for physically correct visibility term)
        float NDF = DistributionGGX(N, H, roughness);
        float Vis = V_GGX(NdotL, NdotV, alphaRoughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 specular = NDF * Vis * F;
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    // Default light if no lights
    if (numLights == 0u) {
        vec3 L = normalize(vec3(0.5, 1.0, 0.3));
        vec3 H = normalize(V + L);
        float NdotL_def = max(dot(N, L), 0.0);
        
        // Use same Cook-Torrance as main loop for consistency
        float NDF = DistributionGGX(N, H, roughness);
        float Vis = V_GGX(NdotL_def, NdotV, alphaRoughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 specular = NDF * Vis * F;
        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
        
        Lo = (kD * albedo / PI + specular) * vec3(1.0) * NdotL_def;
    }
    
    // Procedural sky environment for reflections (same as frag.frag)
    vec3 R = reflect(-V, N);
    float skyGradient = clamp(R.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 skyColor = mix(vec3(0.9, 0.85, 0.8), vec3(0.3, 0.5, 0.95), skyGradient);
    vec3 groundColor = vec3(0.25, 0.22, 0.18);
    vec3 envColor = R.y > 0.0 ? skyColor : groundColor;
    
    // Fresnel for environment reflection (using roughness-aware version)
    vec3 F_env = FresnelSchlickRoughness(NdotV, F0, roughness);
    
    // Metals get stronger environment reflection
    float envStrength = mix(0.3, 1.5, metallic) * (1.0 - roughness * 0.7);
    vec3 envReflection = envColor * F_env * envStrength;
    
    vec3 kD_env = (1.0 - F_env) * (1.0 - metallic);
    vec3 ambientDiffuse = kD_env * albedo * vec3(0.15);
    
    vec3 ambient = ambientDiffuse + envReflection;
    vec3 color = ambient + Lo + emissive;
    
    // ACES tone mapping + sRGB output
    color = toneMapACES(color);
    color = linearToSRGB(color);
    
    outColor = vec4(color, pc.color.a * objData.baseColor.a);
}
