# Plan: Loading and Managers

This document describes the roadmap for the generic loader/job system and the managers module (pipeline, mesh, texture, material). For the draw loop, blend params, and materials see [plan-rendering-and-materials.md](plan-rendering-and-materials.md).

---

## 0. Manager lifecycle (smart pointers) — implemented

Shaders and materials use `shared_ptr` so resources are released when nothing uses them. No manual ref-count or `Release()`.

- **Shaders**: VulkanShaderManager returns `shared_ptr<VkShaderModule>` with a custom deleter; when the last ref drops, the deleter destroys the module. Pipelines hold these shared_ptrs. **TrimUnused()** (e.g. once per frame) removes cache entries where `use_count() == 1`.
- **Pipelines**: PipelineManager stores `shared_ptr<PipelineHandle>` per key; returns `shared_ptr<PipelineHandle>`. **TrimUnused()** moves unused to pending; **ProcessPendingDestroys()** at start of frame (after `vkWaitForFences`) destroys them. **DestroyPipelines()** on swapchain recreate.
- **Materials**: MaterialManager registry material id → `shared_ptr<MaterialHandle>`; material resolves and caches `shared_ptr<PipelineHandle>` via PipelineManager, so materials keep pipelines alive. **TrimUnused()** drops materials no object uses.
- **Meshes**: **MeshHandle** (class) owns `VkBuffer` and `VkDeviceMemory`; destructor destroys them. MeshManager get-or-create procedural by key (vertex buffer upload); RequestLoadMesh + OnCompletedMeshFile parse .obj and upload. Returns `shared_ptr<MeshHandle>`. **TrimUnused()** drops meshes no object uses. **Destroy()** clears cache—call **before** device destroy; call **UnloadScene()** first so no scene refs keep MeshHandles alive.
- **Cleanup**: Single path—`Cleanup()` only. Order: UnloadScene → MeshManager.Destroy() → **TextureManager.Destroy()** → ShaderManager.Destroy() → Device. See [architecture.md](architecture.md).

---

## 1. Generic loader / job system — implemented

- **JobQueue** is a typed job system:
  - Job types: `LoadFile` (path → bytes), `LoadMesh`, `LoadTexture`. Workers read file bytes; decode/upload on main thread.
  - **SubmitLoadFile(sPath)** posts a load-file job and returns a shared result handle (caller can wait). **SubmitLoadTexture(sPath)** posts a texture load (no wait handle).
  - App drains the “completed” queue with type + data.
- **ProcessCompletedJobs(pHandler_ic)** is called each frame to drain the completed queue and **dispatch by type** (e.g. File → shader manager; later Mesh → mesh manager; Texture → texture manager).
- **Shader manager** loads SPIR-V via the same job queue (file load); pipeline/material resolution stays on main thread.
---

## 2. Managers module

- **Location**: `src/managers/` (PipelineManager, MaterialManager); `src/vulkan/` (VulkanShaderManager).
- **VulkanShaderManager**: Returns `shared_ptr<VkShaderModule>` with custom deleter (no manual Release). Cache holds shared_ptrs; TrimUnused() drops shaders no pipeline uses.
- **PipelineManager**: Get-or-create by key; returns `VkPipeline` and `VkPipelineLayout`. Caches `VulkanPipeline` per key. DestroyPipelines() on swapchain recreate.
- **MaterialManager**: Registry material id → `shared_ptr<MaterialHandle>`. Material = pipeline key + layout + rendering state. Resolves to `VkPipeline`/`VkPipelineLayout` via PipelineManager. TrimUnused() drops materials no object uses.
- **Mesh manager** (implemented): **MeshHandle** owns VkBuffer/VkDeviceMemory. Get-or-create procedural by key (triangle, circle, rectangle, cube) with **CreateVertexBufferFromData**; **RequestLoadMesh(path)** + **OnCompletedMeshFile(path, data)** parse .obj and upload vertex buffer. SetDevice/SetPhysicalDevice/SetQueue/SetQueueFamilyIndex before use. SetJobQueue() before RequestLoadMesh. **Destroy()** clears cache (before device; UnloadScene first). TrimUnused(). See [plan-editor-and-scene.md](plan-editor-and-scene.md).
- **SceneManager** (implemented): owns current Scene; LoadSceneAsync(path) via JobQueue, parse JSON on main when ready; UnloadScene, SetCurrentScene, CreateDefaultScene, AddObject/RemoveObject.
- **RenderListBuilder** (implemented): scene + managers → `std::vector<DrawCall>`; sort by (pipeline, mesh). Optional frustum culling (pass viewProj); push constant size validated against material layout. App reuses one vector per frame.
- **Texture manager** (implemented): **TextureHandle** owns VkImage, VkImageView, VkSampler, VkDeviceMemory. Load images via stb_image; **RequestLoadTexture(sPath)** + **OnCompletedTexture(sPath_ic, vecData_in)** from job queue. Get-or-load by path; **TrimUnused()**; **Destroy()** before device. SetDevice/SetPhysicalDevice/SetQueue/SetQueueFamilyIndex and SetJobQueue before use.

**SceneManager**: Owns current **Scene** (objects + name). **LoadSceneAsync(path)** enqueues file load (JobQueue); when the completed job is processed on the main thread, parses JSON (name, objects array: mesh, material, position, color), resolves material/mesh via MaterialManager/MeshManager, sets new scene as current. **UnloadScene()**, **SetCurrentScene()**, **CreateDefaultScene()**, **AddObject** / **RemoveObject**. SetDependencies(JobQueue, MaterialManager, MeshManager). Scene file format: see `scenes/default.json`.

**Dependency**: Shaders → Pipeline → Material → Scene (objects hold shared_ptrs). RenderListBuilder (in app) copies VkPipeline/layout into DrawCall.

---

## 3. Editor and many objects

To support an editor with many objects and different GPU data per object: MeshManager provides mesh ids and draw params; Scene holds renderables (mesh id, material id, per-object data); RenderListBuilder produces the draw list from scene + PipelineManager + MeshManager. Pipeline layout is parameterized per pipeline key so different materials can have different push constant layouts. See [plan-editor-and-scene.md](plan-editor-and-scene.md) for the full phased plan.

---

## 4. Not done yet

- **Descriptor sets**: Infrastructure in place (optional descriptor set layouts in `PipelineLayoutDescriptor`; app creates one layout + pool + one set for combined image sampler). Pipelines can now include descriptor set layouts; next: bind texture to the set, add layout to material pipeline, and bind set in command buffer (materials + textures).
- **Instancing**: RenderListBuilder still emits one draw per object; no grouping by (mesh, material) with instanceCount > 1.

---

## 5. Order of work

1. **Done**: Plan document; managers module scaffold.
2. **Done**: Typed job queue (LoadFile, LoadMesh, LoadTexture); main thread drains each frame and dispatches by type; no lambdas (named handler).
3. **Done**: Pipeline manager (shared_ptr<PipelineHandle>, TrimUnused, ProcessPendingDestroys).
4. **Done**: Phase 1.5 (depth and multi-viewport prep). See [plan-editor-and-scene.md](plan-editor-and-scene.md).
5. **Done**: Smart pointers; MeshManager (procedural + RequestLoadMesh/OnCompletedMeshFile, .obj parse); Scene + SceneManager; **RenderListBuilder** (sort by pipeline/mesh, frustum culling, push size validation, reuse vector). Single Cleanup() path. See §0 and [architecture.md](architecture.md).
6. **Done**: Mesh vertex buffer upload and pipeline vertex input; MeshHandle owns buffers; DrawCall vertex buffer binding; UnloadScene before MeshManager.Destroy() in cleanup. See [plan-editor-and-scene.md](plan-editor-and-scene.md).
7. **Done**: Texture manager (stb_image, TextureHandle, cache, async load, TrimUnused, Destroy before device).
8. **Done**: Descriptor set infrastructure (layout, pool, one set; pipeline layout supports optional descriptor set layouts).
9. **Next**: Materials + textures (bind texture to set, add layout to pipeline, bind set in Record); then instancing.
