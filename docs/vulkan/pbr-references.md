# PBR Implementation References

This document lists the Khronos reference implementations and glTF extensions used as source for our PBR shader implementation.

## Khronos glTF-Sample-Viewer

The official reference renderer for glTF 2.0 with full PBR and extension support.

**Repository:** https://github.com/KhronosGroup/glTF-Sample-Viewer

### Key Shader Files

| File | Purpose | URL |
|------|---------|-----|
| `brdf.glsl` | BRDF functions (D, V, F terms) | [brdf.glsl](https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/main/source/Renderer/shaders/brdf.glsl) |
| `material_info.glsl` | MaterialInfo struct with all extensions | [material_info.glsl](https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/main/source/Renderer/shaders/material_info.glsl) |
| `textures.glsl` | sRGB/Linear conversion, texture sampling | [textures.glsl](https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/main/source/Renderer/shaders/textures.glsl) |
| `tonemapping.glsl` | Tone mapping operators (ACES, Reinhard, etc.) | [tonemapping.glsl](https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/main/source/Renderer/shaders/tonemapping.glsl) |
| `ibl.glsl` | Image-Based Lighting | [ibl.glsl](https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/main/source/Renderer/shaders/ibl.glsl) |
| `punctual.glsl` | Punctual lights (point, spot, directional) | [punctual.glsl](https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/main/source/Renderer/shaders/punctual.glsl) |

### Full Shader Index
https://github.com/KhronosGroup/glTF-Sample-Viewer/tree/main/source/Renderer/shaders

---

## glTF 2.0 Extensions

### KHR_lights_punctual (Implemented ✅)

Point, spot, and directional lights with physically-based attenuation.

**Spec:** https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual

**Key formulas:**
```glsl
// Inverse-square falloff with smooth window
rangeAttenuation = 1.0 / max(distance * distance, 0.0001);
smoothWindow = clamp(1.0 - pow(distance / range, 4.0), 0.0, 1.0);
attenuation = rangeAttenuation * smoothWindow * smoothWindow;
```

**Default values:**
- `intensity`: 1.0 candela (point/spot) or lux (directional)
- `range`: infinite (positive infinity)
- `color`: [1.0, 1.0, 1.0] (white)

---

### KHR_materials_pbrMetallicRoughness (Core ✅)

The base PBR metallic-roughness workflow.

**Spec:** https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material

**Key formulas:**
```glsl
// F0 based on IOR 1.5 (dielectric)
vec3 f0 = vec3(0.04);
f0 = mix(f0, baseColor, metallic);

// alphaRoughness = perceptualRoughness²
float alphaRoughness = roughness * roughness;

// Cook-Torrance specular
specular = D * Vis * F;  // Vis = G / (4 * NdotL * NdotV)

// Lambertian diffuse
diffuse = (1 - F) * (1 - metallic) * baseColor / PI;
```

---

### KHR_materials_unlit (Not Implemented)

Renders material without any lighting calculations. Useful for pre-lit content, UI, or stylized rendering.

**Spec:** https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_unlit

**Implementation:**
```glsl
color = baseColorFactor * baseColorTexture * vertexColor;
```

---

### KHR_materials_ior (Not Implemented)

Custom index of refraction for dielectric materials.

**Spec:** https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_ior

**Key formulas:**
```glsl
// Default IOR = 1.5 gives F0 = 0.04
float f0 = pow((ior - 1.0) / (ior + 1.0), 2.0);

// Common IOR values:
// Water: 1.33 (F0 = 0.02)
// Glass: 1.5  (F0 = 0.04) - default
// Diamond: 2.42 (F0 = 0.17)
```

---

### KHR_materials_emissive_strength (Not Implemented)

HDR emissive materials with intensity > 1.0.

**Spec:** https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_emissive_strength

**Implementation:**
```glsl
emissive = emissiveFactor * emissiveTexture * emissiveStrength;
// emissiveStrength can be > 1.0 for HDR bloom effects
```

---

### KHR_materials_clearcoat (Not Implemented)

Secondary specular layer for car paint, lacquered wood, wet surfaces.

**Spec:** https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_clearcoat

**Key formulas:**
```glsl
// Clearcoat uses fixed IOR 1.5 (F0 = 0.04)
vec3 clearcoatFresnel = F_Schlick(vec3(0.04), VdotH);
float clearcoatBRDF = D_GGX(clearcoatRoughness, NdotH) * V_GGX(clearcoatRoughness, NdotL, NdotV);

// Composite: clearcoat on top, base material underneath
color = mix(baseBRDF, clearcoatBRDF, clearcoatFactor * clearcoatFresnel);
// Energy loss: base layer is dimmed by (1 - clearcoatFactor * clearcoatFresnel)
```

---

### KHR_materials_sheen (Not Implemented)

Fabric and velvet materials with soft edge scattering.

**Spec:** https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_sheen

**Key concept:** Charlie distribution for cylindrical microfibers (instead of GGX for spherical microfacets).

**Implementation:**
```glsl
// Sheen uses different distribution than specular
float D_Charlie = /* Charlie distribution */;
vec3 sheenBRDF = sheenColor * D_Charlie * V_Sheen;

// Albedo scaling to maintain energy conservation
baseColor *= sheenAlbedoScaling;
color = baseBRDF + sheenBRDF;
```

---

### KHR_materials_transmission (Not Implemented)

Transparent materials like glass, thin plastic films.

**Spec:** https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_transmission

**Key formulas:**
```glsl
// BTDF for transmission (refraction)
vec3 transmittedLight = refract(-V, N, 1.0 / ior);
float transmissionBRDF = D_GGX * V_transmission * (1 - F);

// Mix reflection and transmission
color = mix(transmissionColor, reflectionColor, F);
```

---

### KHR_materials_volume (Not Implemented)

Volumetric absorption for thick glass, colored liquids.

**Spec:** https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_volume

**Key formulas:**
```glsl
// Beer-Lambert absorption
vec3 absorption = exp(-attenuationCoefficient * thickness);
transmittedColor *= absorption;

// attenuationColor: color at attenuationDistance
// thickness: distance light travels through medium
```

---

### Other Extensions (Reference Only)

| Extension | Purpose | Spec URL |
|-----------|---------|----------|
| `KHR_materials_specular` | Custom specular color/intensity | [Spec](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_specular) |
| `KHR_materials_iridescence` | Thin-film interference (soap bubbles) | [Spec](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_iridescence) |
| `KHR_materials_anisotropy` | Brushed metal, hair, satin | [Spec](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_anisotropy) |
| `KHR_materials_dispersion` | Rainbow chromatic aberration | [Spec](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_dispersion) |

---

## Implementation Priority

1. **Current:** KHR_materials_pbrMetallicRoughness + KHR_lights_punctual
2. **Next:** KHR_materials_unlit (separate pipeline, trivial)
3. **Next:** KHR_materials_emissive_strength (single float multiply)
4. **Later:** KHR_materials_ior (F0 calculation)
5. **Advanced:** Clearcoat, Sheen, Transmission (require additional textures/uniforms)

---

## Shadows (Future Work)

Our current implementation has no shadow mapping. Options:

1. **Shadow Maps** - Traditional depth-from-light rendering
   - Directional: orthographic projection
   - Point: cube map (6 faces) or dual paraboloid
   - Spot: perspective projection

2. **Cascaded Shadow Maps (CSM)** - For large outdoor scenes

3. **Variance Shadow Maps (VSM)** - Softer shadows, filterable

4. **Ray-traced shadows** - Vulkan RT extension (KHR_ray_tracing_pipeline)

Reference implementation ideas:
- https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
- Filament shadow implementation: https://github.com/google/filament

---

## Color Space Notes

**Critical:** glTF textures use different color spaces:

| Texture Type | Color Space | Conversion Needed |
|-------------|-------------|-------------------|
| baseColorTexture | sRGB | sRGBToLinear() on sample |
| emissiveTexture | sRGB | sRGBToLinear() on sample |
| metallicRoughnessTexture | Linear | None |
| normalTexture | Linear | None |
| occlusionTexture | Linear | None |

**Output:** Final color must be converted linearToSRGB() before display (or use VK_FORMAT_*_SRGB swapchain format for automatic conversion).
