# Shader list and binding layout

Reference for alpha and beyond. See also [alpha_review/questions.txt](../alpha_review/questions.txt) Section 10.

## Current shaders (alpha)

| Shader | Stage | Purpose |
|--------|--------|---------|
| vert.vert | Vertex | Main PBR mesh (MVP, object index, visible indices) |
| frag.frag | Fragment | Main PBR (lights, PBR params, textures) |
| debug_line.vert | Vertex | Debug line draw |
| debug_line.frag | Fragment | Debug line draw |
| gpu_cull.comp | Compute | Frustum culling → visible indices SSBO |

One main PBR program (vert + frag) for all materials.

## Descriptor set 0 — binding table (main PBR)

| Binding | Type | Used in | Description |
|---------|------|---------|-------------|
| 0 | Texture (sampler2D) | frag | Base color |
| 1 | UBO | vert, frag | Global (reserved: time, viewport, exposure) — must be written by C++ |
| 2 | SSBO | vert, frag | Object data (dynamic offset per draw) |
| 3 | SSBO | frag | Light buffer |
| 4 | Texture | frag | Metallic-roughness |
| 5 | Texture | frag | (PBR slot) |
| 6 | Texture | frag | Emissive (default tex if missing) |
| 7 | Texture | frag | Normal (default tex if missing) |
| 8 | SSBO | vert | Visible indices (GPU culler output) |

C++ must write all bindings 0–8 for every descriptor set using the main layout (EnsureMainDescriptorSetWritten, GetOrCreateDescriptorSetForTexture, etc.).

## Planned shaders (post-alpha)

- **Unlit** — non-PBR materials.
- **Shadow** — shadow map pass(es).
- **Sky** — skybox / atmosphere.
- **Post** — post-process (bloom, tone mapping, etc.).

## C++ todos (from alpha review)

- [ ] Add global UBO; write binding 1 in all main-layout descriptor write paths.
- [ ] Add time_demo.vert + time_demo.frag (proof of work for binding 1); one scene cube; integrate in render loop.
- [ ] Fix descriptor writes for bindings 6, 7, 8 (default tex + visible-indices buffer) where still missing.

## Build

Compile with project script (e.g. `scripts/linux/compile_shaders.sh` or CMake shader target). Output: `build/shaders/*.spv`; install copies to `install/shaders/`.
