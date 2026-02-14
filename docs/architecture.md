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
| **VulkanRenderPass** | Render pass (attachments, subpasses). Currently color only; Phase 1.5: create from descriptor (color format + optional depth format). | Recreated when swapchain or target config is recreated. |
| **VulkanPipeline** | Graphics pipeline (vert+frag, params: topology, rasterization, MSAA). Pipeline layout from descriptor (push constant ranges; later descriptor set layouts). Viewport/scissor dynamic. Phase 1.5: optional depth state from params. Depends on render pass. | Recreated when render pass, pipeline params, or layout descriptor change. |
| **PipelineManager** | Get-or-create pipeline by key; stores layout descriptor per key (push ranges, later set layouts). Caller passes `GraphicsPipelineParams` and layout at get/request time. | Pipelines destroyed on swapchain recreate; recreated when shaders ready and GetPipelineIfReady called. |
| **VulkanFramebuffers** | One framebuffer per swapchain image (or per offscreen target). Phase 1.5: create from list of attachment views (color + optional depth) + extent. | Recreated when swapchain or target extent is recreated. |
| **VulkanCommandBuffers** | Command pool + one primary command buffer per swapchain image. `Record(index, renderPass, framebuffer, extent, drawCalls, clearColor)`. Phase 1.5: Record takes render area, viewport, scissor, clear value array (color + depth); multi-viewport-ready. | Recreated when swapchain is recreated. |
| **VulkanSync** | Per-frame-in-flight fences and semaphores (image available, render finished). Render-finished semaphores are per swapchain image. | Recreated when swapchain is recreated (new image count). |
| **Scene / ObjectManager** | List of renderables: each has mesh id, material id, per-object GPU data (e.g. transform). No Vulkan types; editor adds/removes/updates objects. | N/A (data only). |
| **RenderListBuilder** | Builds `std::vector<DrawCall>` from scene: resolve pipeline/layout per object via material, resolve mesh draw params, sort by pipeline (and optionally mesh), optional instancing. Uses PipelineManager and MeshManager. | N/A (stateless or reuses one buffer per frame). |
| **MeshManager** | Load mesh by path (or create procedurally); returns mesh id and draw params (vertex count, first vertex, index count, etc.). Owns vertex/index buffers. | Buffers recreated only on device/context change or explicit reload. |
| **VulkanApp** | Owns all of the above. Init order, main loop, build draw list (or delegate to RenderListBuilder), `DrawFrame(drawCalls)`, `RecreateSwapchainAndDependents()`, cleanup. | Orchestrates when to reconstruct. |

## Init and cleanup order

**Init:** Window → Instance → Window.CreateSurface → Device → Swapchain → RenderPass → Pipeline → Framebuffers → CommandBuffers → Sync.

**Cleanup (reverse):** Sync → CommandBuffers → Framebuffers → Pipeline → RenderPass → Swapchain → Device → Window.DestroySurface → Instance → Window.

## Swapchain extent and aspect ratio

Extent is the single source of truth for how big the swapchain images are and must match what the window displays so the image is not stretched.

- **Source:** At init we call `GetDrawableSize()` and set `m_config.lWidth/lHeight` before creating the swapchain (no config size that might differ from the real window). On resize we set config from `GetDrawableSize(newW, newH)` and recreate. When applying config (e.g. from file), we use that config and recreate.
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
|            | `present_mode` | `fifo` \| `mailbox` \| `immediate` \| `fifo_relaxed` |
|            | `preferred_format` | e.g. `B8G8R8A8_SRGB`, `B8G8R8A8_UNORM`, `R8G8B8A8_SRGB` |
|            | `preferred_color_space` | `SRGB_NONLINEAR`, `DISPLAY_P3`, `EXTENDED_SRGB` |

Validation layers are **not** in the config file (dev/debug only); enabled when `ENABLE_VALIDATION_LAYERS` is set (e.g. debug build). See [vulkan/validation-layers.md](vulkan/validation-layers.md).

- **Resize path**: User resizes window → config is synced from window size → recreate.
- **Config path**: Load JSON or set config, call `ApplyConfig(config)` → window size/fullscreen/title updated, `bSwapchainDirty` set → next frame recreate uses new config.

See [vulkan/swapchain-rebuild-cases.md](vulkan/swapchain-rebuild-cases.md).

See also [plan-rendering-and-materials.md](plan-rendering-and-materials.md) and [plan-editor-and-scene.md](plan-editor-and-scene.md).

## Rendering and draw list

- **Draw list**: Each frame the app (or a RenderListBuilder) produces a `std::vector<DrawCall>`. Each `DrawCall` holds: pipeline, pipeline layout, optional push constant data (pointer + size), and draw params (vertex count, instance count, first vertex, first instance). No hardcoded pipeline or single draw; multiple objects with different pipelines and push data are supported.
- **Material**: Conceptually “how to draw”—maps to a pipeline key and a pipeline layout descriptor. Objects reference a material id; when building the draw list, material id resolves to pipeline + layout; per-object data (e.g. transform) is the push constant source.
- **Scene**: List of renderables (mesh id, material id, per-object GPU data). Editor or game code adds/edits objects; the list is the single source of truth for “what to draw.” Pipeline layout definition stays in pipeline land; the scene only holds references (material id) and data to push.

## Future extensions (not implemented yet)

- **Phase 1.5: Depth and multi-viewport prep** — Render pass descriptor (color + optional depth), framebuffers with attachment list, depth image, pipeline depth state, Record(render area, viewport, scissor, clear array). See [plan-editor-and-scene.md](plan-editor-and-scene.md). Enables depth for 3D and prepares for ImGui multi-viewport.
- **Phase 2** — MeshManager, material notion, Scene, RenderListBuilder. See [plan-editor-and-scene.md](plan-editor-and-scene.md).
- **3D and camera** — Orthographic is used for debug/editor; the final main view should use perspective (see `ObjectSetPerspective` and `usePerspective` in the app). View matrix from camera position is already applied.
- **Multiple cameras and viewports** — The design should support as many cameras as the user wants. Each camera can have its own projection (ortho or perspective) and view; each may render to its own target or share the swapchain. A second viewport (e.g. to show where the main camera is and what it is looking at) or depth visualization are planned: same render-area/viewport/scissor parameterization and optional extra passes (sample depth texture, draw into a subregion) will support this without changing the core recording API.
- **Shadow maps** — Offscreen render targets (images + framebuffers + pipeline); separate from main swapchain, owned by a dedicated module.
- **Raytracing** — Acceleration structures, raytracing pipeline; separate module, composed in VulkanApp.

New features should be added as new modules (or new submodules under existing ones) rather than overloading a single class.
