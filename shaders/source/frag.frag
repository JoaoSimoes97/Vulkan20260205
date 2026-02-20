#version 450

/*
 * PBR Fragment Shader - Metallic-Roughness Workflow
 * 
 * Reference implementations:
 * - Khronos glTF-Sample-Viewer: https://github.com/KhronosGroup/glTF-Sample-Viewer/tree/main/source/Renderer/shaders
 *   - brdf.glsl:          D, V, F functions (GGX distribution, Smith visibility, Schlick fresnel)
 *   - material_info.glsl: MaterialInfo struct with all extension support
 *   - textures.glsl:      sRGB/Linear conversion
 *   - tonemapping.glsl:   ACES, Reinhard, Uncharted2
 *   - punctual.glsl:      Point/spot/directional light attenuation
 * 
 * - glTF 2.0 Spec: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
 * - KHR_lights_punctual: https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual
 * 
 * See docs/vulkan/pbr-references.md for full extension list and formulas.
 */

/* ---- Push Constants ---- */
/* LAYOUT VALIDATION (must match C++ kPushOffset_* in object.h):
 * Offset 0:  mat4 mvp        (64 bytes)
 * Offset 64: vec4 color      (16 bytes)
 * Offset 80: uint objectIndex (4 bytes)
 * Offset 84: padding         (12 bytes) - _pad0, _pad1, _pad2
 * Offset 96: vec4 camPos     (16 bytes) - MUST be 16-byte aligned for vec4
 * Total: 112 bytes
 */
layout(push_constant) uniform Push {
    mat4 mvp;           // Model-View-Projection (unused in frag) - offset 0
    vec4 color;         // Per-object tint color - offset 64
    uint objectIndex;   // Index into object data SSBO - offset 80
    uint _pad0;         // Padding for 16-byte alignment of camPos - offset 84
    uint _pad1;         // Padding - offset 88
    uint _pad2;         // Padding (total 12 bytes padding) - offset 92
    vec4 camPos;        // Camera world position (xyz), w unused - offset 96
} pc;

/* ---- Texture Sampler (binding 0) ---- */
layout(set = 0, binding = 0) uniform sampler2D uTex;

/* ---- Metallic-Roughness Texture (binding 4) ---- */
/* glTF spec: Green channel = roughness, Blue channel = metallic */
layout(set = 0, binding = 4) uniform sampler2D uMetallicRoughnessTex;

/* ---- Emissive Texture (binding 5) ---- */
/* glTF spec: RGB emissive color, multiplied by emissiveFactor from material */
layout(set = 0, binding = 5) uniform sampler2D uEmissiveTex;

/* ---- Normal Texture (binding 6) ---- */
/* glTF spec: Tangent-space normal map (RGB where XYZ = normal * 0.5 + 0.5) */
layout(set = 0, binding = 6) uniform sampler2D uNormalTex;

/* ---- Occlusion Texture (binding 7) ---- */
/* glTF spec: Ambient occlusion in red channel */
layout(set = 0, binding = 7) uniform sampler2D uOcclusionTex;

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
// Reference: https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/main/source/Renderer/shaders/textures.glsl
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
// Reference: https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/main/source/Renderer/shaders/tonemapping.glsl
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

// Smith Joint GGX Visibility (Khronos reference)
// Note: Vis = G / (4 * NdotL * NdotV)
// From Eric Heitz 2014 and Filament
float V_GGX(float NdotL, float NdotV, float alphaRoughness) {
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    
    float GGX = GGXV + GGXL;
    return GGX > 0.0 ? 0.5 / GGX : 0.0;
}

// Fresnel-Schlick
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness (for IBL/environment reflections)
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// KHR_lights_punctual range attenuation with inverse-square falloff
// From: https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual
float CalcAttenuation(float distance, float range, float falloff) {
    // Inverse-square falloff (physically based)
    float distanceSquared = distance * distance;
    float rangeAttenuation = 1.0 / max(distanceSquared, 0.0001);
    
    // Smooth window function to avoid hard cutoff at range
    // smoothAttenuation = max(min(1.0 - (distance/range)^4, 1), 0)^2
    if (range > 0.0) {
        float d_over_r = distance / range;
        float d_over_r_4 = d_over_r * d_over_r * d_over_r * d_over_r;
        float smoothWindow = clamp(1.0 - d_over_r_4, 0.0, 1.0);
        rangeAttenuation *= smoothWindow * smoothWindow;
    }
    
    return rangeAttenuation;
}

// Spotlight cone attenuation
float CalcSpotAttenuation(vec3 L, vec3 spotDir, float innerCone, float outerCone) {
    float theta = dot(L, normalize(-spotDir));
    float epsilon = cos(innerCone) - cos(outerCone);
    return clamp((theta - cos(outerCone)) / max(epsilon, 0.0001), 0.0, 1.0);
}

void main() {
    // Get per-object data
    ObjectData objData = objectData.objects[inObjectIndex];
    
    // Sample texture with UV wrapping
    vec2 uv = fract(inUV);
    vec4 texColor = texture(uTex, uv);
    
    // Sample metallic-roughness texture (glTF: G=roughness, B=metallic)
    // Note: metallic-roughness is stored in LINEAR space per glTF spec
    vec4 mrTex = texture(uMetallicRoughnessTex, uv);
    
    // Sample emissive texture (glTF: RGB emissive, multiplied by emissiveFactor)
    // Emissive textures are in sRGB space - must convert to linear
    vec3 emissiveTex = sRGBToLinear(texture(uEmissiveTex, uv).rgb);
    
    // Material properties: factor * texture (per glTF spec)
    // Base color textures are in sRGB space - must convert to linear for PBR calculations
    vec3 albedo = sRGBToLinear(texColor.rgb) * sRGBToLinear(pc.color.rgb) * sRGBToLinear(objData.baseColor.rgb);
    float metallic = clamp(objData.matProps.x * mrTex.b, 0.0, 1.0);
    float roughness = clamp(objData.matProps.y * mrTex.g, MIN_ROUGHNESS, 1.0);
    // glTF spec: emissive = emissiveFactor * emissiveTexture (emissive.a unused, was for 'strength')
    vec3 emissive = objData.emissive.rgb * emissiveTex;
    
    // Normal mapping using screenspace-derived TBN (works without vertex tangents)
    vec3 N = normalize(inNormal);
    {
        // Sample normal texture (glTF: tangent-space normal, XYZ = normal * 0.5 + 0.5)
        vec3 normalTex = texture(uNormalTex, uv).rgb;
        // Check if normal map is valid (not default white texture = (1,1,1))
        float normalScale = objData.matProps.z;
        if (normalTex != vec3(1.0, 1.0, 1.0) && normalScale > 0.0) {
            // Convert from [0,1] to [-1,1] range
            vec3 tangentNormal = normalTex * 2.0 - 1.0;
            tangentNormal.xy *= normalScale;
            tangentNormal = normalize(tangentNormal);
            
            // Screenspace-derived TBN (cotangent frame from position and UV derivatives)
            vec3 dp1 = dFdx(inWorldPos);
            vec3 dp2 = dFdy(inWorldPos);
            vec2 duv1 = dFdx(uv);
            vec2 duv2 = dFdy(uv);
            
            // Solve linear system for tangent/bitangent
            vec3 dp2perp = cross(dp2, N);
            vec3 dp1perp = cross(N, dp1);
            vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
            vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
            
            // Construct TBN matrix (inverse scale for cotangent frame)
            float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
            mat3 TBN = mat3(T * invmax, B * invmax, N);
            
            // Transform normal from tangent space to world space
            N = normalize(TBN * tangentNormal);
        }
    }
    
    // Camera position from push constants (passed each frame)
    vec3 V = normalize(pc.camPos.xyz - inWorldPos);
    
    // Base reflectance (F0)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    // Accumulate lighting
    vec3 Lo = vec3(0.0);
    
    uint numLights = min(lightData.lightCount, MAX_LIGHTS);
    
    for (uint i = 0u; i < numLights; ++i) {
        GpuLight light = lightData.lights[i];
        
        // Skip inactive lights
        if (light.params.w < 0.5)
            continue;
        
        vec3 L;
        float attenuation = 1.0;
        float lightType = light.direction.w;
        
        if (lightType < 0.5) {
            // Directional light
            L = normalize(-light.direction.xyz);
            attenuation = 1.0;
        } else if (lightType < 1.5) {
            // Point light
            vec3 lightVec = light.position.xyz - inWorldPos;
            float distance = length(lightVec);
            L = normalize(lightVec);
            attenuation = CalcAttenuation(distance, light.position.w, light.params.z);
        } else {
            // Spot light
            vec3 lightVec = light.position.xyz - inWorldPos;
            float distance = length(lightVec);
            L = normalize(lightVec);
            attenuation = CalcAttenuation(distance, light.position.w, light.params.z);
            attenuation *= CalcSpotAttenuation(L, light.direction.xyz, light.params.x, light.params.y);
        }
        
        vec3 H = normalize(V + L);
        vec3 radiance = light.color.rgb * light.color.a * attenuation;
        
        // Cook-Torrance BRDF (Khronos reference implementation)
        float NdotL = max(dot(N, L), 0.0);
        float NdotV = max(dot(N, V), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float VdotH = max(dot(V, H), 0.0);
        
        // alphaRoughness = perceptualRoughness² (Khronos convention)
        float alphaRoughness = roughness * roughness;
        
        float NDF = DistributionGGX(N, H, roughness);
        float Vis = V_GGX(NdotL, NdotV, alphaRoughness);
        vec3 F = FresnelSchlick(VdotH, F0);
        
        // specular = D * Vis (Vis already includes G/(4*NdotL*NdotV))
        vec3 specular = vec3(NDF * Vis) * F;
        
        // Energy conservation
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic; // Metals have no diffuse
        
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    // Default light if no lights in scene
    if (numLights == 0u) {
        vec3 L = normalize(vec3(0.5, 1.0, 0.3));
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        
        // Simplified PBR for default light
        vec3 diffuse = albedo * NdotL;
        float spec = pow(max(dot(N, H), 0.0), mix(8.0, 128.0, 1.0 - roughness));
        vec3 specular = vec3(spec) * mix(vec3(0.04), albedo, metallic);
        
        Lo = diffuse * 0.8 + specular * 0.3;
    }
    
    // Ambient lighting with procedural environment reflection (fake IBL)
    // Apply ambient occlusion from texture (glTF: red channel, multiplied by occlusionStrength)
    float ao = texture(uOcclusionTex, uv).r;
    float occlusionStrength = objData.matProps.w;
    ao = mix(1.0, ao, occlusionStrength); // Blend based on strength (0 = no AO, 1 = full AO)
    
    // Procedural sky environment for reflections
    // Compute reflection vector
    vec3 R = reflect(-V, N);
    
    // Sky gradient based on reflection direction (simulates outdoor environment)
    float skyGradient = clamp(R.y * 0.5 + 0.5, 0.0, 1.0); // 0 at horizon, 1 at zenith
    vec3 skyColor = mix(vec3(0.9, 0.85, 0.8), vec3(0.3, 0.5, 0.95), skyGradient); // Warm horizon to vivid sky blue
    vec3 groundColor = vec3(0.25, 0.22, 0.18); // Ground brown  
    vec3 envColor = R.y > 0.0 ? skyColor : groundColor;
    
    // Fresnel for environment reflection (more reflection at glancing angles)
    vec3 F_env = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    
    // Environment reflection - balance with direct lighting
    // Lower values let punctual light specular show through better
    float envStrength = mix(0.1, 0.35, metallic) * (1.0 - roughness * 0.8);
    vec3 envReflection = envColor * F_env * envStrength;
    
    // Ambient diffuse for non-metals
    vec3 kD_env = (1.0 - F_env) * (1.0 - metallic);
    vec3 ambientDiffuse = kD_env * albedo * vec3(0.15); // Soft ambient light
    
    vec3 ambient = (ambientDiffuse + envReflection) * ao;
    
    // Final color (all calculations in linear space)
    vec3 color = ambient + Lo + emissive;
    
    // Tone mapping (ACES filmic - better highlight handling than Reinhard)
    color = toneMapACES(color);
    
    // NOTE: No linearToSRGB() here - swapchain is VK_FORMAT_B8G8R8A8_SRGB
    // which automatically converts linear output to sRGB for display
    
    outColor = vec4(color, texColor.a * pc.color.a * objData.baseColor.a);
}