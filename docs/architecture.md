# Architecture and modules

The app is split into modules so it stays maintainable and can grow (multiple cameras, shadow maps, raytracing, etc.) without one giant file.

## Module layout

| Module | Responsibility | Reconstruction |
|--------|----------------|----------------|
| **VulkanConfig** | Runtime options: resolution, present mode, vsync, etc. Changing options sets `bSwapchainDirty` to trigger rebuild. | Config is input to `RecreateSwapchain`. |
| **Window** | SDL init, window, events. Creates Vulkan surface. Exposes `GetFramebufferResized`, `GetWindowMinimized`, `GetDrawableSize`. | Resize/minimize set flags; swapchain module reacts. |
| **VulkanInstance** | Instance, layers, extensions (e.g. from SDL). | Rarely recreated. |
| **VulkanDevice** | Physical device pick, logical device, queues, queue family indices. | Usually fixed; only on device lost / GPU switch. |
| **VulkanSwapchain** | Swapchain, image views, extent, format, present mode. `RecreateSwapchain(config)` tears down and recreates. | Core of reconstruction (resize, present mode, format change). |
| **VulkanRenderPass** | Render pass (attachments, subpasses). Create from descriptor (color format + optional depth format, load/store ops). | Recreated when swapchain or target config is recreated. |
| **VulkanDepthImage** | Depth image + view for render pass attachment. Created from device, physical device, format, extent. `FindSupportedFormat` for format selection. | Recreated when extent changes (with swapchain). |
| **VulkanPipeline** | Graphics pipeline (vert+frag, params: topology, rasterization, depth state, **blend state**, MSAA). Pipeline layout from descriptor (push constant ranges; later descriptor set layouts). Viewport/scissor dynamic. | Recreated when render pass, pipeline params, or layout descriptor change. |
| **PipelineManager** | Get-or-create pipeline by key; returns `VkPipeline` and `VkPipelineLayout` when shaders are ready. Caches `VulkanPipeline` per key. **DestroyPipelines()** on swapchain recreate. | Pipelines destroyed on swapchain recreate. |
| **MaterialManager** | Registry: material id → `shared_ptr<MaterialHandle>`. Material = pipeline key + layout + rendering state. Resolves to `VkPipeline`/`VkPipelineLayout` via PipelineManager. **TrimUnused()** drops materials no object uses. | Materials released when ref count 0 after trim. |
| **VulkanFramebuffers** | One framebuffer per swapchain image (or per offscreen target). Create from list of attachment views (color + optional depth) + extent. | Recreated when swapchain or target extent is recreated. |
| **VulkanCommandBuffers** | Command pool + one primary command buffer per swapchain image. `Record(...)` binds vertex buffers per draw then records draw. Each `DrawCall` includes `vertexBuffer`, `vertexBufferOffset`, draw params. | Recreated when swapchain is recreated. |
| **VulkanSync** | Per-frame-in-flight fences and semaphores (image available, render finished). Render-finished semaphores are per swapchain image. | Recreated when swapchain is recreated (new image count). |
| **Scene** | Container for objects (and later lights): `std::vector<Object>`, name. **Clear()** drops refs so managers can TrimUnused. Data only; no Vulkan types. | N/A (data only). |
| **SceneManager** | Owns current scene; **LoadSceneAsync(path)** (file load via JobQueue, parse on main when ready); **UnloadScene()**, **SetCurrentScene()**, **CreateDefaultScene()**; **AddObject** / **RemoveObject**. SetDependencies(JobQueue, MaterialManager, MeshManager) before use. | Scene swap on load; unload drops refs. |
| **RenderListBuilder** | Builds `std::vector<DrawCall>` from scene: resolve pipeline/layout per object via material, mesh → draw params; **sorts by (pipeline, mesh)** to reduce binds. Optional **frustum culling** (pass viewProj); **push constant size** validated against material layout. App reuses one vector per frame. | N/A (stateless). |
| **VulkanShaderManager** | Load SPIR-V via job queue; returns `shared_ptr<VkShaderModule>` with custom deleter (no manual Release). Cache holds shared_ptrs; **TrimUnused()** drops shaders no pipeline uses. | Shaders released when ref count 0; deleter destroys module. |
| **TextureManager** | **TextureHandle** (class) owns VkImage, VkImageView, VkSampler, VkDeviceMemory. Load images via stb_image; **RequestLoadTexture(sPath)** + **OnCompletedTexture(sPath_ic, vecData_in)** from JobQueue. Get-or-load by path; **TrimUnused()**; **Destroy()** before device. SetDevice/SetPhysicalDevice/SetQueue/SetQueueFamilyIndex and SetJobQueue before use. | Destroy() before device; UnloadScene not required (textures not yet referenced by materials). |
| **MeshManager** | **MeshHandle** (class) owns `VkBuffer` and `VkDeviceMemory`; destructor destroys them. **SetDevice** / **SetPhysicalDevice** / **SetQueue** / **SetQueueFamilyIndex** before use. **GetOrCreateProcedural(key)** builds vertex buffers (triangle, circle, rectangle, cube); **RequestLoadMesh(path)** + **OnCompletedMeshFile(sPath_ic, vecData_in)** parse .obj and upload vertex buffer. Caches `shared_ptr<MeshHandle>` by key/path. **Destroy()** clears cache (call before device destroy). **TrimUnused()** when no object uses mesh. | UnloadScene() before Destroy() so no scene refs keep MeshHandles alive. |
| **Camera** | World-space position; **GetViewMatrix(out16)** writes column-major view matrix (translate by -position). Config supplies initial position. | Data only; no Vulkan. |
| **CameraController** | **CameraController_Update(camera, keyState, panSpeed)** updates camera position from keyboard (WASD / arrows / QE). Key state from SDL_GetKeyboardState; pan speed from config. | Stateless; app calls each frame. |
| **VulkanApp** | Owns window, Vulkan stack, managers (including **TextureManager**), scene, **Camera**. Main loop: **ProcessCompletedJobs(completedJobHandler)** (no lambdas; named handler), TrimUnused (shader, pipeline, material, mesh, texture), PollEvents, **CameraController_Update**, resize/swapchain check, build projection + **camera.GetViewMatrix** → viewProj, FillPushData, **RenderListBuilder.Build(..., viewProj)** (frustum culling), **DrawFrame** (always, empty list = clear only), FPS title. **RecreateSwapchainAndDependents()**, **Cleanup()**. | Orchestrates init, loop, and teardown. |

## Init and cleanup order

**Init:** Window → Instance → Window.CreateSurface → Device → Swapchain → RenderPass → DepthImage (if depth) → Framebuffers → CommandBuffers → Sync.

**Cleanup (reverse, single path):** Sync → CommandBuffers → Framebuffers → DepthImage → Pipelines (DestroyPipelines) → RenderPass → Swapchain → **SceneManager.UnloadScene()** (drops scene refs to MeshHandles) → **MeshManager.Destroy()** → **TextureManager.Destroy()** → **ShaderManager** (so all VkShaderModules are destroyed) → Device → Window.DestroySurface → Instance → Window → JobQueue. Only `Cleanup()` tears down; exit loop then `Run()` calls `Cleanup()`.

## Manager lifecycle: smart pointers, release when unused

Asset managers use `shared_ptr` so resources are released when nothing uses them; no manual ref-count or `Release()`.

- **Shaders**: VulkanShaderManager caches `shared_ptr<VkShaderModule>` with a custom deleter (destroys module when last ref drops). Pipelines hold these shared_ptrs. **TrimUnused()** removes cache entries where `use_count() == 1`. No RAII handle struct—just shared_ptr + deleter.
- **Pipelines**: PipelineManager stores `VulkanPipeline` per key; returns raw `VkPipeline`/`VkPipelineLayout`. Pipelines are destroyed in **DestroyPipelines()** (e.g. on swapchain recreate). No pipeline trim in the hot path (pipelines may still be in use by submitted command buffers).
- **Materials**: MaterialManager registry material id → `shared_ptr<MaterialHandle>`. Material resolves to `VkPipeline`/`VkPipelineLayout` via PipelineManager. **TrimUnused()** drops materials no object uses.
- **Draw list**: Each `DrawCall` holds pipeline, pipeline layout, vertex buffer + offset, draw params (vertex count, instance count, first vertex, first instance), and push constant data. Built each frame from scene: material → pipeline/layout, mesh → vertex buffer and draw params. **VulkanCommandBuffers::Record** binds vertex buffers before each draw.
- **Cleanup order**: Unload scene first so no objects hold `shared_ptr<MeshHandle>`. Then MeshManager.Destroy() clears its cache and all MeshHandle destructors run (destroying VkBuffer/VkDeviceMemory). Shader manager must be destroyed **before** the device so all `VkShaderModule`s are freed. See Init and cleanup order above.

See [ROADMAP.md](ROADMAP.md) Phase 2–3 for the implementation plan.

## Resource cleanup and async management

**ResourceCleanupManager** centralizes cleanup of all asset caches (materials, meshes, textures, pipelines, shaders). It provides a single orchestrator interface to trim unused resources and is invoked asynchronously by **ResourceManagerThread** so cleanup does not block the main rendering thread.

**Thread safety:**
- **MaterialManager**, **MeshManager**, **TextureManager**, **PipelineManager** all use `std::shared_mutex` to protect concurrent access.
  - Read operations (e.g., `GetMaterial()`, `GetMesh()`) use `std::shared_lock` to allow multiple concurrent readers.
  - Write operations (e.g., `TrimUnused()`, cache registration, destruction) use `std::unique_lock` for exclusive access.
- **ShaderManager** protects shader cache and pending load state similarly.

**Async cleanup workflow:**
1. **Main thread** during each frame calls `m_resourceManagerThread.EnqueueCommand(TrimAll, TrimAllCaches)` (non-blocking).
2. **Worker thread** waits on an idle timeout (10 ms by default when queue is empty).
3. **Worker thread** dequeues the TrimAll command and calls `ResourceCleanupManager::TrimAllCaches()`.
4. **ResourceCleanupManager** iterates each registered manager and trims caches:
   - `TrimMaterials()`: removes materials where `use_count() == 1` (no objects hold the only reference).
   - `TrimMeshes()`: removes meshes no material references.
   - `TrimTextures()`: removes textures no material references.
   - `TrimPipelines()`: removes pipelines no material uses.
   - Can also trim shaders (if enabled via `SetTrimShaders(true)`).
5. **Worker thread** sleeps again. Meanwhile, **main thread** continues rendering (GPU processes submitted commands).
6. **Main thread** after GPU idle (vkWaitForFences) calls `ProcessPendingDestroys()` on each manager to safely destroy resources that were moved to the pending list during trim.

**Design rationale:**
- Trimming caches is relatively cheap (O(n) scan of shared_ptrs) but adds up over time when many objects are loaded/unloaded. Moving it to a worker thread eliminates frame hiccups.
- `ProcessPendingDestroys()` runs on the main thread after GPU idle to guarantee no in-flight command buffers or pipelines are being destroyed.
- Each manager can be individually enabled/disabled for trimming via `ResourceCleanupManager::SetTrimX()` methods, allowing fine-grained control during profiling or specialized scenarios.

**Performance impact:**
- Main thread enqueue: ~0.01 ms.
- Worker thread trim + pending destroy: ~650 µs total (while GPU processes prior frames).
- Net result: **zero added main-thread latency** to frame rendering.

See [plan-loading-and-managers.md](plan-loading-and-managers.md) for detailed resource lifecycle and loading strategy.

## Swapchain extent and aspect ratio

Extent is the single source of truth for how big the swapchain images are and must match what the window displays so the image is not stretched.

- **Source:** At init we call `GetDrawableSize()` and set `m_config.lWidth/lHeight` before creating the swapchain. **Every frame** we compare current drawable size to swapchain extent and recreate when they differ (catches resize even if the window event was missed). `RecreateSwapchainAndDependents()` also refreshes config from `GetDrawableSize()` so OUT_OF_DATE-driven recreate uses the latest size. When applying config (e.g. from file), we use that config and set `bSwapchainDirty` for next-frame recreate.
- **ChooseExtent** (in `VulkanSwapchain`): Uses only the requested extent. If it is within the surface’s `[minImageExtent, maxImageExtent]`, we use it as-is. If it is outside (e.g. driver limits), we fit into that range while **preserving aspect ratio** so the image is never stretched.
- **Viewport:** Viewport and scissor are dynamic and set in the command buffer at record time from the same extent we use for the render area, so there is no pipeline vs framebuffer extent mismatch.

Logging: init and resize paths log drawable size and swapchain extent at INFO so you can confirm they match.

## When swapchain is recreated

- Window resize / fullscreen / display change → `bFramebufferResized` (or config) → before next draw: `RecreateSwapchainAndDependents()`.
- User changes present mode / vsync / resolution in config → `bSwapchainDirty` → same path.
- In `DrawFrame`, `VK_ERROR_OUT_OF_DATE_KHR` from acquire or present → `RecreateSwapchainAndDependents()` and retry.

## Config and JSON file (all options runtime-applicable)

Config uses **two files**: a read-only default and a user config that can be updated.

- **default.json** (`config/default.json`): Single source of default values. Created once by the app if missing; **never overwritten**. Do not edit for normal use.
- **config.json** (`config/config.json`): User config. Created from default if missing; can be updated by the app. Edit this to change settings.

**On startup:** The app calls `LoadConfigFromFileOrCreate("config/config.json", "config/default.json")`. Paths are relative to the current working directory. User config is **merged over** default (missing keys in user = value from default). If the driver does not support a requested option (e.g. present mode or surface format), the app **fails with a clear log message**; the user adjusts config and restarts (no silent fallback).

**JSON structure** (see `config_loader.h`). Uses **nlohmann/json**.

| Section    | Keys | Values / type |
|------------|------|----------------|
| `window`   | `width`, `height` | unsigned int |
|            | `fullscreen` | bool |
|            | `title` | string |
| `swapchain`| `image_count` | 2 or 3 (double/triple buffering); driver must return this exact count or app fails |
|            | `max_frames_in_flight` | e.g. 2; must be at least 1 (0 is treated as 1) |
|            | `present_mode` | `fifo` (vsync) \| `mailbox` \| `immediate` (no vsync, default) \| `fifo_relaxed` |
|            | `preferred_format` | e.g. `B8G8R8A8_SRGB`, `B8G8R8A8_UNORM`, `R8G8B8A8_SRGB` |
|            | `preferred_color_space` | `SRGB_NONLINEAR`, `DISPLAY_P3`, `EXTENDED_SRGB` |
| `camera`   | `use_perspective` | bool (perspective vs orthographic) |
|            | `fov_y_rad`, `near_z`, `far_z` | float (perspective) |
|            | `ortho_half_extent`, `ortho_near`, `ortho_far` | float (ortho) |
|            | `pan_speed` | float (WASD/arrows/QE move speed) |
|            | `initial_camera_x`, `initial_camera_y`, `initial_camera_z` | float (world position at startup) |
| `render`   | `cull_back_faces` | bool |
|            | `clear_color_r`, `clear_color_g`, `clear_color_b`, `clear_color_a` | float (0–1) |

Validation layers are **not** in the config file (dev/debug only); enabled when `ENABLE_VALIDATION_LAYERS` is set (e.g. debug build). See [vulkan/validation-layers.md](vulkan/validation-layers.md).

- **Resize path**: User resizes window → config is synced from window size → recreate.
- **Config path**: Load JSON or set config, call `ApplyConfig(config)` → window size/fullscreen/title updated, `bSwapchainDirty` set → next frame recreate uses new config.

See [vulkan/swapchain-rebuild-cases.md](vulkan/swapchain-rebuild-cases.md). For config file location and editing see [getting-started.md](getting-started.md) (Config). For aspect/resize issues see [troubleshooting.md](troubleshooting.md).

See also [ROADMAP.md](ROADMAP.md) for the complete phase breakdown.

## Rendering and draw list

- **Draw list**: Each frame the app (or a RenderListBuilder) produces a `std::vector<DrawCall>`. Each `DrawCall` holds: pipeline, pipeline layout, vertex buffer + offset, draw params (vertex count, instance count, first vertex, first instance), and optional push constant data (pointer + size). Built from scene: material → pipeline/layout, mesh → vertex buffer and draw params. Record binds vertex buffers before each draw. No hardcoded pipeline or single draw; multiple objects with different pipelines and push data are supported.
- **Material**: Conceptually “how to draw”—maps to a pipeline key and a pipeline layout descriptor. Objects reference a material id; when building the draw list, material id resolves to pipeline + layout; per-object data (e.g. transform) is the push constant source.
- **Scene**: Container (objects, optional name). Each object holds shared_ptr to material and mesh plus per-object data (transform, color). **SceneManager** owns current scene; LoadSceneAsync(path), UnloadScene, SetCurrentScene, CreateDefaultScene, AddObject/RemoveObject. The list is the single source of truth for “what to draw.” Pipeline layout definition stays in pipeline land; the scene only holds references (material id) and data to push.

## Implemented (Phase 1.5 and camera)

- **Depth and multi-viewport prep** — Render pass descriptor (color + optional depth), `VulkanDepthImage`, framebuffers with attachment list, pipeline depth state from params, `Record(renderArea, viewport, scissor, clearValues, clearValueCount)`. Enables depth for 3D and prepares for multiple viewports (e.g. ImGui).
- **Camera and projection** — Config-driven: perspective or ortho, FOV, near/far, ortho params, pan speed, initial camera position. View matrix from camera position; aspect = width/height; resize syncs swapchain to drawable size every frame so aspect stays correct.

## Future extensions (not implemented yet)

- **Phase 2** — MeshManager, MaterialManager, Scene, SceneManager, draw list from scene: **implemented**. See [ROADMAP.md](ROADMAP.md) Phase 2. Scene file format: JSON with `name` and `objects` array (mesh, material, position, color); load via **LoadSceneAsync** (JobQueue).
- **Multiple cameras and viewports** — The design should support as many cameras as the user wants. Each camera can have its own projection (ortho or perspective) and view; each may render to its own target or share the swapchain. A second viewport (e.g. to show where the main camera is and what it is looking at) or depth visualization are planned: same render-area/viewport/scissor parameterization and optional extra passes (sample depth texture, draw into a subregion) will support this without changing the core recording API.
- **Shadow maps** — Offscreen render targets (images + framebuffers + pipeline); separate from main swapchain, owned by a dedicated module.
- **Raytracing** — Acceleration structures, raytracing pipeline; separate module, composed in VulkanApp.

New features should be added as new modules (or new submodules under existing ones) rather than overloading a single class.
