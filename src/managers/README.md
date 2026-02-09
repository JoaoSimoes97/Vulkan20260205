# Managers module

Managers own cached resources and depend on the job queue / completed results. They run on the main thread; workers only do I/O and CPU work.

## Pipeline manager

- **Role**: Request pipelines by key (vert + frag paths); non-blocking. `RequestPipeline(key, ...)` submits shader loads; `GetPipelineIfReady(key, ...)` returns `VkPipeline` when shaders are ready. Supports multiple pipelines in parallel; no waits.
- **Depends on**: Shader manager (for `VkShaderModule`s). Render pass and extent (for pipeline create).
- **Used by**: Drawables (e.g. cubes). When a cube needs to draw, it asks for the “cube” pipeline; pipeline manager returns it (or creates it when shaders are ready).

## Shader manager

- **Lives in**: `vulkan/` (VulkanShaderManager). Not moved here.
- **Role**: Load SPIR-V on demand via job queue; cache `VkShaderModule`; ref-count and unload when ref 0.
- **Used by**: Pipeline manager.

## Mesh manager (future)

- **Role**: Get-or-load mesh by path. Submits LoadMesh job; worker reads and parses (e.g. .obj); main thread creates vertex/index buffers and caches by path. Ref-count and unload when no longer used.
- **Used by**: Drawables (cubes, terrain, etc.). One mesh asset can be shared by many instances.

## Texture manager (future)

- **Role**: Get-or-load texture by path. Submits LoadTexture job; worker reads and decodes image; main thread creates `VkImage`, uploads, creates view + sampler. Cache and ref-count.
- **Used by**: Materials / pipelines that need a texture binding.

## Dependency

Shaders → Pipeline → Drawable (Pipeline + Mesh (+ Texture)).  
Multiple cubes = one pipeline + one mesh asset + N instances.
