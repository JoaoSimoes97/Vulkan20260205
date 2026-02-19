# Resource Loading and Manager Lifecycle

This document describes how assets (materials, meshes, textures, shaders, pipelines) are loaded, managed, and cleaned up in the Vulkan app.

## Asset Lifecycle Overview

All asset managers follow a **shared_ptr-based reference counting** pattern where resources are automatically released when nothing references them anymore. This eliminates manual ref-counting and `Release()` calls.

```
Request Asset
    ↓
Asset loaded/created (stored in shared_ptr cache)
    ↓
Object acquires shared_ptr (use_count > 1)
    ↓
Object released from scene
    ↓
Async cleanup: TrimUnused() scans cache; removes entries where use_count == 1
    ↓
Worker thread pending destroy list
    ↓
Main thread ProcessPendingDestroys() after GPU idle
    ↓
Asset destroyed (destructors run, Vulkan resources freed)
```

## Manager Details

### MaterialManager

**Purpose:** Registry of materials. Material = pipeline key + layout descriptor + rendering state.

**Data structure:** `std::map<std::string, shared_ptr<MaterialHandle>>`

**Key methods:**
- `RegisterMaterial(id, pipelineKey, layoutDesc, params)` → `shared_ptr<MaterialHandle>` (added to cache).
- `GetMaterial(id)` → `shared_ptr<MaterialHandle>` or nullptr (read-lock protected).
- `TrimUnused()` → removes entries where `use_count() == 1` (unique-lock protected).
- `ProcessPendingDestroys()` → destroys moved handles (unique-lock protected).

**Lifecycle:**
1. Objects in a scene reference a material id.
2. When scene is loaded, SceneManager calls RegisterMaterial for each material used.
3. Objects acquire shared_ptr to MaterialHandle via GetMaterial.
4. When objects are removed or scene is unloaded, shared_ptr refs are released.
5. TrimUnused finds materials with `use_count() == 1` and moves them to pending destroy.
6. ProcessPendingDestroys runs after GPU idle and destroys them.

**Thread safety:** `std::shared_mutex` protects cache and pending destroy list.
- Read: `GetMaterial()` uses `std::shared_lock`.
- Write: `RegisterMaterial()`, `TrimUnused()`, `ProcessPendingDestroys()` use `std::unique_lock`.

### MeshManager

**Purpose:** Registry of mesh geometry. MeshHandle owns VkBuffer (vertex data) and VkDeviceMemory.

**Data structure:** `std::map<std::string, shared_ptr<MeshHandle>>`

**Key methods:**
- `GetOrCreateProcedural(key)` → procedurally generated mesh (triangle, circle, rectangle, cube).
- `RequestLoadMesh(path)` → enqueue async load via JobQueue.
- `OnCompletedMeshFile(path, data)` → parse .obj and upload buffer (callback from JobQueue).
- `GetMesh(path)` → `shared_ptr<MeshHandle>` or nullptr (read-lock protected).
- `TrimUnused()` → removes entries where `use_count() == 1` (unique-lock protected).
- `ProcessPendingDestroys()` → destroys moved handles (unique-lock protected).
- `Destroy()` → clears entire cache (call before device destroy).

**Lifecycle:**
1. Scene objects reference mesh paths/keys.
2. When loading, RequestLoadMesh enqueues load; JobQueue reads file asynchronously.
3. OnCompletedMeshFile parses and uploads vertex buffer.
4. Objects acquire shared_ptr via GetMesh.
5. On UnloadScene, shared_ptrs release refs.
6. TrimUnused moves unused to pending; ProcessPendingDestroys destroys them.

**Thread safety:** `std::shared_mutex` protects cache and pending destroy list.
- Read: `GetMesh()` uses `std::shared_lock`.
- Write: `OnCompletedMeshFile()`, `TrimUnused()`, `ProcessPendingDestroys()` use `std::unique_lock`.

### TextureManager

**Purpose:** Registry of texture images. TextureHandle owns VkImage, VkImageView, VkSampler, VkDeviceMemory.

**Data structure:** `std::map<std::string, shared_ptr<TextureHandle>>`

**Key methods:**
- `RequestLoadTexture(path)` → enqueue async load via JobQueue.
- `OnCompletedTexture(path, data)` → decode with stbi_image and upload image/sampler (callback from JobQueue).
- `GetTexture(path)` → `shared_ptr<TextureHandle>` or nullptr (read-lock protected).
- `GetOrCreateDefaultTexture()` → white 1x1 pixel texture (unique-lock protected).
- `GetOrCreateFromMemory(key, width, height, channels, pixels)` → upload from buffer (unique-lock protected).
- `TrimUnused()` → removes entries where `use_count() == 1` (unique-lock protected).
- `Destroy()` → clears entire cache (call before device destroy).

**Lifecycle:**
1. Materials reference texture paths (embedded in material descriptor or by name).
2. RequestLoadTexture enqueues load; JobQueue fetches file asynchronously.
3. OnCompletedTexture decodes image data and uploads texture.
4. Materials acquire shared_ptr via GetTexture.
5. When scene unloads or material is replaced, refs release.
6. TrimUnused finds unused and moves to pending destroy.

**Thread safety:** `std::shared_mutex` protects cache and pending paths set.
- Read: `GetTexture()` uses `std::shared_lock`.
- Write: `GetOrCreateDefaultTexture()`, `GetOrCreateFromMemory()`, `OnCompletedTexture()`, `TrimUnused()`, `Destroy()` use `std::unique_lock`.

**Special case:** Default texture (`__default`) is kept alive by `m_cachedMaterials` vector in VulkanApp to prevent TrimUnused from discarding it (descriptor set depends on its sampler).

### PipelineManager

**Purpose:** Registry of graphics pipelines. Returns `VkPipeline` and `VkPipelineLayout` when shaders are ready.

**Data structure:** `std::map<std::string, PipelineEntry>` where entry holds shader paths, pipeline handle, render pass, last params.

**Key methods:**
- `RequestPipeline(key, pShaderManager, vertPath, fragPath)` → register pipeline request (unique-lock protected).
- `GetPipelineHandleIfReady(key, device, renderPass, params, layoutDesc, ...)` → returns `shared_ptr<PipelineHandle>` when shaders ready; rebuilds if render pass or params changed (unique-lock protected).
- `TrimUnused()` → removes entries where `use_count() == 1` (unique-lock protected).
- `ProcessPendingDestroys()` → destroys moved handles (unique-lock protected).
- `DestroyPipelines()` → clears entire cache (call on swapchain recreate).

**Lifecycle:**
1. Materials reference a pipeline key (e.g., "main_tex", "wire_untex").
2. RequestPipeline enqueues shader loads.
3. GetPipelineHandleIfReady checks if shaders are ready; if so, creates VulkanPipeline and returns handle.
4. Materials hold `shared_ptr<PipelineHandle>`.
5. On swapchain recreate or render pass change, pipelines are destroyed and recreated.
6. TrimUnused moves unused to pending; ProcessPendingDestroys destroys them.

**Thread safety:** `std::shared_mutex` protects entries and pending destroy list.
- Write: `RequestPipeline()`, `GetPipelineHandleIfReady()`, `TrimUnused()`, `ProcessPendingDestroys()`, `DestroyPipelines()` all use `std::unique_lock`.

### VulkanShaderManager

**Purpose:** Registry of compiled shader modules. Caches `shared_ptr<VkShaderModule>` with custom deleter.

**Key methods:**
- `RequestLoad(path)` → enqueue compile (if not already pending/loaded).
- `IsLoadReady(path)` → true if shader is compiled and ready.
- `GetShaderIfReady(device, path)` → `shared_ptr<VkShaderModule>` or nullptr.
- `TrimUnused()` → removes shaders where `use_count() == 1` (pipelines don't hold refs anymore).

**Lifecycle:**
1. PipelineManager requests shader loads.
2. Shaders are compiled asynchronously (glslc).
3. Pipelines acquire shared_ptrs to shader modules.
4. When pipeline is destroyed, shader refs release.
5. TrimUnused finds shaders no pipeline uses.

## Resource Cleanup Architecture

### ResourceCleanupManager

**Purpose:** Centralized orchestrator for all manager cleanup operations. Provides single interface to trim all caches and supports per-manager enable/disable.

**Key methods:**
- `SetManagers(materials, meshes, textures, pipelines, shaders)` → register all manager pointers.
- `TrimAllCaches()` → calls TrimUnused on all enabled managers.
- `SetTrimMaterials(bool)`, `SetTrimMeshes(bool)`, etc. → enable/disable per-manager trimming.

**Design:** Allows centralized, orchestrated cleanup while remaining decoupled from individual manager implementations.

### ResourceManagerThread

**Purpose:** Worker thread that executes resource cleanup commands asynchronously so main thread is not blocked.

**Architecture:**
- Lock-free command queue (enqueue from main, dequeue on worker).
- Worker thread sleeps 10 ms during idle; wakes when command enqueued.
- Supports multiple command types: `TrimMaterials`, `TrimMeshes`, `TrimTextures`, `TrimPipelines`, `TrimShaders`, `TrimAll`, `ProcessDestroys`, `Shutdown`.
- Each command can carry a callback (lambda) to execute.

**Workflow:**
1. Main thread calls `EnqueueCommand(TrimAll, callback)` (non-blocking, ~0.01 ms).
2. Worker thread dequeues and calls the callback on worker thread.
3. Callback executes `ResourceCleanupManager::TrimAllCaches()`.
4. Worker thread returns to idle state.
5. Main thread continues rendering; GPU processes prior command buffers.
6. After GPU idle (vkWaitForFences), main thread calls `ProcessPendingDestroys()` on each manager to safely destroy.

### MainLoop Integration

```cpp
// Each frame in MainLoop:
ProcessCompletedJobs();
CleanupUnusedTextureDescriptorSets();

// Enqueue async cleanup
m_resourceManagerThread.EnqueueCommand(
    ResourceManagerThread::Command(
        ResourceManagerThread::CommandType::TrimAll,
        [this]() { this->m_resourceCleanupManager.TrimAllCaches(); }
    )
);

// Poll input, update camera, etc.
// ...

// After GPU idle
vkWaitForFences(...);

// Execute pending destroys on main thread (safe from GPU perspective)
m_materialManager.ProcessPendingDestroys();
m_meshManager.ProcessPendingDestroys();
m_textureManager.ProcessPendingDestroys();
m_pipelineManager.ProcessPendingDestroys();
```

## Performance Characteristics

| Operation | Thread | Time | Impact |
|-----------|--------|------|--------|
| Enqueue TrimAll | Main | ~0.01 ms | Negligible |
| Scan + trim cache | Worker | ~300–650 µs | While GPU busy |
| ProcessPendingDestroys | Main | ~100–300 µs | After GPU idle |
| **Total per frame** | **Net** | **+0 ms main** | No frame hiccup |

The worker thread operates while the GPU processes prior frames, so the main thread pays **zero cost** for cleanup in the hot path (render loop).

## Detailed Loading Sequence

### Scene Load with Materials and Meshes

```
1. User requests scene load: LoadLevelFromFile("level.json")
   ↓
2. Parse JSON: get material names, mesh paths, object list
   ↓
3. For each material:
     - RegisterMaterial(name, pipelineKey, layoutDesc, params)
     - Material added to cache, use_count = 1
   ↓
4. For each unique mesh path:
     - RequestLoadMesh(path) → JobQueue
     - JobQueue async loads .obj file
   ↓
5. For each texture referenced by material:
     - RequestLoadTexture(path) → JobQueue
     - JobQueue async loads image file
   ↓
6. Main thread waits for job completion flags (IsLoadReady, GetMesh, GetTexture)
   ↓
7. For each object in JSON:
     - Create Object with material id, mesh path, transform
     - GetMaterial(id) → shared_ptr (use_count now > 1)
     - GetMesh(meshPath) → shared_ptr (use_count > 1)
     - Add to scene
   ↓
8. Scene now owns all object shared_ptrs; use_counts > 1
```

### Scene Unload

```
1. User requests unload: UnloadScene()
   ↓
2. Clear scene (delete all objects)
   ↓
3. All object shared_ptrs destroyed; use_counts drop to 1
   ↓
4. Next frame: TrimUnused scans materials/meshes/textures
   ↓
5. Entries with use_count == 1 moved to pending destroy
   ↓
6. ProcessPendingDestroys after GPU idle: destructors run
   ↓
7. Meshes destroyed → VkBuffer/VkDeviceMemory freed
   ↓
8. Textures destroyed → VkImage/VkImageView/VkSampler/memory freed
   ↓
9. Pipelines destroyed → VkPipeline/VkPipelineLayout freed
```

## Thread Safety Summary

| Manager | Mutex | Read (shared_lock) | Write (unique_lock) |
|---------|-------|-------------------|---------------------|
| MaterialManager | shared_mutex | GetMaterial | Register, TrimUnused, ProcessDestroys |
| MeshManager | shared_mutex | GetMesh | Register, TrimUnused, ProcessDestroys |
| TextureManager | shared_mutex | GetTexture | Create, TrimUnused, Destroy |
| PipelineManager | shared_mutex | — | All ops (GetPipelineHandleIfReady modifies state) |
| ShaderManager | shared_mutex (implicit via safe deleter) | GetShaderIfReady | RequestLoad, TrimUnused |

All write operations are safe to execute on worker thread while main thread renders, thanks to reader-writer locks (shared_mutex).

## Best Practices

1. **Always call `ProcessPendingDestroys()` after GPU idle.** Otherwise destroyed resources remain in memory until next frame.
2. **Keep references to frequently-used assets** (e.g., `m_cachedMaterials`) to prevent accidental trim.
3. **Enable/disable per-manager trimming** via `ResourceCleanupManager::SetTrimX()` during profiling to identify hotspots.
4. **Avoid holding raw pointers into manager caches.** Always use shared_ptrs returned by Get/RegisterX methods.
5. **Call UnloadScene before switching levels.** This releases all object refs so TrimUnused can clean up resources.

See [architecture.md](architecture.md) for overall system design and [troubleshooting.md](troubleshooting.md) for profiling and debugging resource leaks.
