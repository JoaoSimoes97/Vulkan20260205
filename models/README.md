# glTF Sample Models

This directory contains glTF models for testing and demonstration.

## Models:

### DamagedHelmet.glb (3.6 MB)
- **Source**: Khronos glTF Sample Models
- **Features**: Full PBR workflow
  - Base color texture (albedo)
  - Metallic-roughness texture
  - Normal map
  - Ambient occlusion
  - Emissive texture
- **Geometry**: 14,556 vertices, 46,356 indices
- **License**: Creative Commons Attribution-NonCommercial (CC BY-NC)
- **Author**: theblueturtle_ (via Sketchfab)

### BoxTextured.glb (6.4 KB)
- **Source**: Khronos glTF Sample Models
- **Features**: Simple textured cube
  - Base color texture
  - UVs and normals
- **Geometry**: Simple box mesh
- **License**: Public domain / CC0

## Usage:

Reference these models in level JSON files:
```json
{
  "source": "../../models/DamagedHelmet.glb",
  "position": [0.0, 0.0, -2.0],
  "scale": [1.0, 1.0, 1.0]
}
```

## Testing Coverage:

These models test:
- ✅ Texture loading (baseColorTexture)
- ✅ UV coordinate extraction
- ✅ Normal extraction
- ✅ Indexed vs non-indexed geometry
- ✅ Embedded textures in GLB
- ✅ PBR material properties (metallic, roughness)
- ✅ Multiple primitives per mesh
