# PBR Shader Requirements

This document describes the requirements for objects to render correctly with the PBR pipeline.

## Overview

The engine uses a **Cook-Torrance BRDF** with GGX distribution for physically-based rendering. The primary shader is `frag.frag` (compiled to `frag.spv`), which is the only fully up-to-date PBR shader.

## Object Requirements

For an object to render correctly with PBR, it needs:

### Textures (all optional, defaults provided)

| Texture | Binding | Default | Purpose |
|---------|---------|---------|---------|
| `pTexture` | 0 | White (1,1,1,1) | Base color / albedo |
| `pMetallicRoughnessTexture` | 4 | White (1,1,1,1) | G=roughness, B=metallic |
| `pEmissiveTexture` | 5 | White (1,1,1,1) | Self-illumination RGB |
| `pNormalTexture` | 6 | White (1,1,1,1) | Tangent-space normal map |
| `pOcclusionTexture` | 7 | White (1,1,1,1) | Ambient occlusion (R channel) |

### Material Factors

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `metallicFactor` | float | 0.0 | 0 = dielectric, 1 = metal |
| `roughnessFactor` | float | 0.5 | 0.04 = smooth, 1.0 = rough |
| `normalScale` | float | 1.0 | Normal map strength |
| `occlusionStrength` | float | 1.0 | AO effect strength |
| `emissive[4]` | vec4 | (0,0,0,0) | RGB + strength multiplier |
| `baseColor[4]` | vec4 | (1,1,1,1) | RGBA tint over base texture |

### SSBO Data (ObjectData structure)

The shader reads per-object data from a Storage Buffer (binding 2):

```glsl
struct ObjectData {
    mat4 model;      // Model matrix
    vec4 emissive;   // RGB + strength
    vec4 matProps;   // x=metallic, y=roughness, z=normalScale, w=occlusion
    vec4 baseColor;  // RGBA base color factor
    vec4 reserved0-8; // Reserved for future use
};
```

## Shader Status

| Shader | Status | Notes |
|--------|--------|-------|
| `frag.frag` | ✅ **Current** | Full PBR with V_GGX, ACES tone mapping, procedural sky IBL |
| `frag_untextured.frag` | ⚠️ **Outdated** | Uses old `GeometrySmith` instead of `V_GGX`; hardcoded camPos |
| `frag_alt.frag` | ℹ️ **Debug Only** | Grayscale debug output, not PBR |
| `debug_line.frag` | ℹ️ **Debug Only** | Wireframe visualization |

## Migration Guide

### For New Objects

Always use the **textured pipeline** (`main_tex`, `wire_tex`, etc.) even for untextured objects:

```cpp
// Use textured pipeline with default textures
obj.pTexture = textureManager->GetOrCreateDefaultTexture();
obj.pMetallicRoughnessTexture = textureManager->GetOrCreateDefaultMRTexture();
obj.pEmissiveTexture = textureManager->GetOrCreateDefaultEmissiveTexture();
obj.pNormalTexture = textureManager->GetOrCreateDefaultNormalTexture();
obj.pOcclusionTexture = textureManager->GetOrCreateDefaultOcclusionTexture();
```

### For Emissive Objects

Emissive values are multiplied by the emissive texture. To have self-glow without a texture:
- Use `GetOrCreateDefaultTexture()` (white) for `pEmissiveTexture`
- Set `emissive[0-2]` to desired RGB values
- Set `emissive[3]` to strength/intensity

**Note**: Currently emissive objects only self-glow. They do not illuminate other objects. See the emissive lighting system below.

## TODO: Shader Updates Needed

The following shaders need to be updated to match `frag.frag`:

1. **frag_untextured.frag**:
   - Replace `GeometrySmith()` with `V_GGX()`
   - Use `pc.camPos` instead of hardcoded camera position
   - Add ACES tone mapping
   - Consider deprecating in favor of textured pipeline with defaults

2. **frag_alt.frag**:
   - Low priority - debug shader only
   - Keep as-is for grayscale debugging

## Emissive Lighting (Future Feature)

Currently emissive surfaces only produce self-glow. To make emissive objects illuminate the scene:

**Recommended Approach**: Auto-generate point lights from emissive objects
- Scan objects where `length(emissive.rgb) * emissive.w > threshold`
- Create corresponding point lights in the light buffer
- Light position = object center, color = emissive color, intensity = emissive strength
- Radius estimated from object bounding sphere

This approach requires no shader changes and integrates with the existing PBR lighting system.
