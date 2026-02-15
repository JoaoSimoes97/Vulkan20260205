# Managers module

Managers own cached resources and depend on the job queue / completed results. They run on the main thread; workers only do I/O and CPU work.

## Pipeline manager

- **Role**: Request pipelines by key (vert + frag paths). Returns `shared_ptr<PipelineHandle>` via **GetPipelineHandleIfReady(key, ...)**. **TrimUnused()** moves unused to pending; **ProcessPendingDestroys()** at start of frame (after fence wait). **DestroyPipelines()** on swapchain recreate.
- **Depends on**: Shader manager. Render pass and extent (for pipeline create).
- **Used by**: MaterialManager (materials cache pipeline handles); draw list gets VkPipeline/VkPipelineLayout from materials.

## Material manager

- **Role**: Registry material id → `shared_ptr<MaterialHandle>`. Material = pipeline key + layout + params; resolves and caches `shared_ptr<PipelineHandle>`. **TrimUnused()** drops materials no object uses.
- **Used by**: Scene objects (each holds shared_ptr<MaterialHandle>); app builds draw list from scene.

## Mesh manager

- **Role**: Get-or-create procedural by key; **RequestLoadMesh(path)** for async file load (JobQueue). **OnCompletedMeshFile(path, data)** parses .obj (vertex/index counts), caches `shared_ptr<MeshHandle>` by path. **SetJobQueue()** before RequestLoadMesh. Vertex buffer upload and pipeline vertex input are future work. **TrimUnused()**.
- **Used by**: Scene objects (mesh key or path); app dispatches completed file loads to OnCompletedMeshFile.

## Scene manager

- **Role**: Owns current **Scene** (objects + name). **LoadSceneAsync(path)** — file load via JobQueue, parse JSON on main when completed; **UnloadScene()**, **SetCurrentScene()**, **CreateDefaultScene()**; **AddObject** / **RemoveObject**. **SetDependencies(JobQueue, MaterialManager, MeshManager)** before use. Scene file format: JSON with `name` and `objects` array (mesh, material, position, color).
- **Used by**: App (get current scene, build draw list, load/unload levels).

## Shader manager

- **Lives in**: `vulkan/` (VulkanShaderManager). Not moved here.
- **Role**: Load SPIR-V on demand via job queue; cache `shared_ptr<VkShaderModule>` with custom deleter (no manual Release). **TrimUnused()** drops shaders no pipeline uses.
- **Used by**: Pipeline manager.

## Texture manager (future)

- **Role**: Get-or-load texture by path. Submits LoadTexture job; worker reads and decodes image; main thread creates `VkImage`, uploads, creates view + sampler. Cache and ref-count.
- **Used by**: Materials / pipelines that need a texture binding.

## Render list builder (render/)

- **Role**: Build `std::vector<DrawCall>` from scene: resolve material → pipeline/layout, mesh → draw params; **sort by (pipeline, mesh)**. App reuses one vector per frame.
- **Used by**: VulkanApp (Build then DrawFrame).

## Dependency

Shaders → Pipeline (PipelineHandle) → Material (MaterialHandle) → Scene (Object holds material + mesh).  
SceneManager owns current scene; mesh and scene file loads async via JobQueue. RenderListBuilder produces sorted draw list; multiple objects = shared materials/meshes + N instances.
