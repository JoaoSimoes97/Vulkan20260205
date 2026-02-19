# Vulkan Project Roadmap

Complete development plan: completed phases, current work, and future direction.

---

## Executive Summary

This Vulkan rendering engine is built in **modular phases**. The foundation (swapchain, render passes, pipelines) is stable. Scene rendering with meshes and materials is functional. Animation and optimization features are planned.

| Phase | Status | Focus |
|-------|--------|-------|
| **1–1.5** | ✅ DONE | Foundation: Instance, device, swapchain, render pass, depth, pipelines, command buffers |
| **2** | ✅ DONE | Scene: Materials, meshes, drawing many objects, job queue, texture loading, async resource cleanup |
| **2.5** | ✅ DONE | Resource Management: Thread-safe managers, async cleanup, ResourceCleanupManager orchestrator |
| **3** | 📋 PLANNED | Materials and Textures: UBOs, material properties, advanced shader binding |
| **4** | 📋 PLANNED | Optimization: Instancing, indirect buffers, GPU-driven culling |
| **5** | 🚀 FUTURE | Animation/Skinning, advanced features |

---

## Phase 1: Foundation ✅

**Goal:** Stable Vulkan core (window, device, swapchain, render pass, simple pipelines, draw loop).

**Completed:**
- ✅ Window creation (SDL3)
- ✅ Vulkan instance, physical device selection, logical device
- ✅ Swapchain creation and resize handling (synced to drawable size every frame)
- ✅ Render pass (color + optional depth attachment)
- ✅ Basic graphics pipeline with vertex/fragment shaders
- ✅ Command buffers, synchronization (fences, semaphores)
- ✅ Draw loop: acquire → record → submit → present
- ✅ Out-of-date and resize handling (RecreateSwapchainAndDependents)

**Key modules:**
- VulkanInstance, VulkanDevice, VulkanSwapchain
- VulkanRenderPass, VulkanFramebuffers
- VulkanCommandBuffers (Record path with render area, viewport, scissor, clear values)
- VulkanSync (per-frame-in-flight synchronization)

---

## Phase 1.5: Depth and Multi-Viewport Prep ✅

**Goal:** Multi-attachment rendering (color + depth); prepare recording path for future multi-viewport.

**Completed:**
- ✅ Render pass descriptor (specify color and optional depth formats, load/store ops)
- ✅ VulkanDepthImage (create/destroy, format selection via FindSupportedFormat)
- ✅ Framebuffers with attachment list (color + depth views)
- ✅ Pipeline depth/stencil state
- ✅ Clear value arrays in Record (one clear per attachment)
- ✅ Record path supports multi-viewport (dynamic viewport, scissor per draw)

**Key insight:** Recording infrastructure is stable and ready for content (meshes, materials, scene).

---

## Phase 2: Scene, Assets, and Drawing ✅

**Goal:** Support many objects with different meshes, materials, and GPU data. Async loading, smart resource management.

**Completed:**

### Core Managers
- ✅ **PipelineManager**: Get VkPipeline/VkPipelineLayout by key; shared_ptr with TrimUnused
- ✅ **MaterialManager**: Registry (material id → MaterialHandle); resolves to pipeline + layout
- ✅ **MeshManager**: MeshHandle owns VkBuffer/VkDeviceMemory
  - Procedural meshes (triangle, circle, rectangle, cube, sphere, cylinder, cone)
  - Load from .obj files (RequestLoadMesh + OnCompletedMeshFile)
  - Vertex buffer upload (position, UV, normal)
  - TrimUnused and Destroy (before device, after UnloadScene)
- ✅ **TextureManager**: TextureHandle owns VkImage/VkImageView/VkSampler/VkDeviceMemory
  - Load images via stb_image
  - Async load (RequestLoadTexture + OnCompletedTexture)
  - TrimUnused, Destroy before device

### Scene and Loading
- ✅ **Scene**: Container (objects, name); Clear() drops refs
- ✅ **SceneManager**: 
  - LoadSceneAsync (JobQueue file load → JSON parse on main)
  - UnloadScene (drop scene refs so managers can trim)
  - SetCurrentScene, CreateDefaultScene
  - AddObject / RemoveObject
- ✅ **JobQueue**: Typed async jobs (LoadFile, LoadMesh, LoadTexture)
  - ProcessCompletedJobs each frame; no lambdas (named handler)

### Drawing
- ✅ **RenderListBuilder**: 
  - Build draw list from scene (object → material/mesh → DrawCall)
  - Sort by (pipeline, mesh) to minimize state changes
  - Optional frustum culling (pass viewProj matrix)
  - Push constant size validation (skip oversized)
  - Reuse single vector per frame (no allocations in hot path)
- ✅ **VulkanCommandBuffers**: Record from DrawCall list
  - Bind vertex buffer per draw

### Resource Lifecycle
- ✅ Smart pointers (shared_ptr) with custom deleters
  - Shaders: VulkanShaderManager caches shared_ptr<VkShaderModule>
  - Materials: MaterialManager holds shared_ptr<MaterialHandle>
  - Meshes/Textures: Return shared_ptr; caller is a user
- ✅ TrimUnused() once per frame (shaders, pipelines, materials, meshes, textures)
- ✅ Single Cleanup() path: UnloadScene → Managers → Device (see architecture.md)

### Configuration
- ✅ JSON config (resolution, vsync, clear color, camera, fulls creen)
- ✅ Default + user config files (config/default.json, config/config.json)
- ✅ Runtime config changes (set bSwapchainDirty to apply, recreate swapchain)
- ✅ Camera (perspective + orthographic, pan speed, initial position)

### Descriptor System
- ✅ **DescriptorSetLayoutManager**: Register layouts by key
- ✅ **DescriptorPoolManager**: Allocate/free descriptor sets
- ✅ **DrawCall**: Multiple descriptor sets, support for texture binding
- ✅ **RenderListBuilder**: Pass pipeline→set mapping; Draw calls filled with sets
- ✅ App: Manage layouts/pool; pass map to Build()

**Key deliverable:** Scene-driven rendering; many objects; async load; automatic resource cleanup; sorting for efficiency.

---

## Phase 2.5: Resource Management and Async Cleanup ✅

**Goal:** Eliminate frame hiccups from resource cleanup; add thread safety to all manager caches.

**Completed:**

### Thread Safety
- ✅ **MaterialManager**: `std::shared_mutex` protecting cache and pending destroy list
  - Read: `GetMaterial()` uses `std::shared_lock` for concurrent readers
  - Write: `RegisterMaterial()`, `TrimUnused()`, `ProcessPendingDestroys()` use `std::unique_lock`
- ✅ **MeshManager**: `std::shared_mutex` with shared/unique locks pattern
- ✅ **TextureManager**: `std::shared_mutex` protecting all cache operations
- ✅ **PipelineManager**: `std::shared_mutex` for all entry and pending destroy operations
- ✅ **VulkanShaderManager**: Thread-safe shader cache with custom deleter

### Async Resource Cleanup
- ✅ **ResourceManagerThread**: 
  - Lock-free command queue for main→worker communication
  - Worker thread with 10 ms idle timeout
  - Supports: TrimMaterials, TrimMeshes, TrimTextures, TrimPipelines, TrimShaders, TrimAll, ProcessDestroys, Shutdown
  - Non-blocking enqueue (~0.01 ms)
- ✅ **ResourceCleanupManager**: 
  - Centralized orchestrator for all manager cleanup
  - SetManagers() registers all asset managers
  - TrimAllCaches() calls TrimUnused on enabled managers
  - Per-manager enable/disable via SetTrimX() methods
- ✅ **MainLoop Integration**:
  - Replaced per-manager TrimUnused calls with single TrimAll command
  - Worker thread executes cleanup while main thread renders
  - ProcessPendingDestroys() still runs on main thread after GPU idle (safe from GPU perspective)

**Performance achieved:**
- Main thread enqueue: ~0.01 ms
- Worker thread trim: ~300–650 µs total
- Net: **Zero added main-thread latency** to rendering

**Key insight:** Cleanup is now fully asynchronous and thread-safe, eliminating potential frame stutters and allowing concurrent reads.

---

## Phase 3: Materials and Textures (Next) 

**Goal:** Full material system with textures, UBOs, blend states, proper shader material binding.

**Planning notes** (material system design):
- Material = appearance (albedo, roughness, metallic) + rendering state (blend, depth, cull)
- Rendering state → GraphicsPipelineParams → pipeline choice (existing)
- Appearance scalars/vectors → UBOs or push constants; shaders read them
- Textures → Descriptor sets (image view + sampler); bind before draw
- Per-object data → push constants or per-instance buffers
- Multiple pipelines → batch by pipeline to minimize binds (RenderListBuilder already sorts)

**Estimated scope:**
- Bind textures in descriptor sets (infrastructure done; just need material assignments)
- UBO management (per-frame, per-object transforms)
- Shader material constants (roughness, metallic, etc.) via push or UBO
- Material config from JSON or glTF

---

## Phase 4: Animation and Skinning 🎯 PLANNED

**Goal:** Full glTF animation and skeletal skinning support.

**Scope:**
- Parse glTF animations (clips, samplers, channels) and skins (joints, inverse bind matrices)
- CPU-side skinning: keyframe interpolation, joint matrix computation
- GPU-side skinning: vertex shader path, joint matrix UBO/SSBO
- Runtime: clip selection, playback control, loop

**Roadmap (see docs/future-ideas/animation-skinning-roadmap.md):**

| Milestone | Scope |
|-----------|-------|
| **M1** | Parse and validate glTF animations + skins; build internal structures; debug logs |
| **M2** | CPU evaluation: time update, keyframe interpolation, hierarchy update, joint matrices |
| **M3** | GPU skinning: vertex format, shader variant, joint matrix upload, render |
| **M4** | Editor controls: clip selection, play/pause, speed, loop; debug viz |

**Current status:** Code logs warnings when animated/skinned glTF is loaded (reminder to implement).

---

## Phase 5: Optimization (Future)

**Goal:** Reduce CPU overhead and improve GPU utilization for large scenes.

### Option A: Instancing
- Group objects by (mesh, material)
- Emit one DrawCall with instanceCount > 1
- Instance data (color, transform, etc.) from per-frame instance buffer
- Reduces draw count; better for scenes with many similar objects

### Option B: Indirect Buffers
- GPU buffer holding draw parameters (vertex count, instance count, offset, etc.)
- CPU records one `vkCmdDrawIndirect` with drawCount
- GPU reads buffer and executes many draws
- Reduces CPU command recording and submission cost
- Enables GPU-driven culling (compute shader generates indirect buffer)

**When to use:**
- **Instancing:** Many similar objects (trees, buildings, particles)
- **Indirect:** Very high draw count (thousands+) or GPU-driven culling desired
- **Both:** Complex scenes (groups of repeated objects + culling)

See [docs/future-ideas/indirect-buffers.md](future-ideas/indirect-buffers.md) for details.

---

## Feature Status Matrix

| Feature | Phase | Status | Notes |
|---------|-------|--------|-------|
| **Window + SDL3** | 1 | ✅ Done | Cross-platform (Linux, Windows, macOS) |
| **Vulkan Device** | 1 | ✅ Done | Instance, physical device, logical device, queues |
| **Swapchain** | 1 | ✅ Done | Resizable, format/present mode selection, triple buffering |
| **Render Pass** | 1 | ✅ Done | Color + depth, configurable load/store |
| **Pipelines** | 1 | ✅ Done | Graphics pipelines, parameterized layout (push constants), blend state |
| **Depth** | 1.5 | ✅ Done | Depth image, format selection, multi-attachment rendering |
| **Draw Loop** | 1 | ✅ Done | Acquire → record → submit → present; out-of-date handling |
| **Shaders** | 1/2 | ✅ Done | Vertex + fragment; SPIR-V compilation; shader caching |
| **Meshes** | 2 | ✅ Done | Procedural (cube, sphere, etc.); .obj loading; vertex buffers |
| **Materials** | 2 | ✅ Done | Material registry, pipeline resolution, TrimUnused |
| **Textures** | 2 | ✅ Done | Image loading (stb_image), async load, descriptor sets, samplers |
| **Scene** | 2 | ✅ Done | Object list, materials, meshes, serialization (JSON) |
| **Config** | 2 | ✅ Done | JSON config (resolution, vsync, camera, clear color) |
| **Descriptors** | 2 | ✅ Done | Layout + pool managers, pipeline layout with sets |
| **Job Queue** | 2 | ✅ Done | Async file/mesh/texture load; main-thread process |
| **GLM Math** | 2 | ✅ Done | Vector/matrix ops, pi constant, view/projection matrices |
| **Camera** | 2 | ✅ Done | Perspective + orthographic, pan speed, position |
| **Frustum Culling** | 2 | ✅ Optional | Optional in RenderListBuilder when viewProj passed |
| **Resource Manager Thread** | 2.5 | ✅ Done | Async cleanup, lock-free command queue, worker thread |
| **Resource Cleanup Manager** | 2.5 | ✅ Done | Centralized cleanup orchestrator, per-manager enable/disable |
| **Thread Safety (Managers)** | 2.5 | ✅ Done | shared_mutex on all managers (Material, Mesh, Texture, Pipeline, Shader) |
| **Material System** | 3 | 📋 Planned | UBOs, appearance constants, shader material binding |
| **Animation** | 4 | 📋 Planned | glTF clips, skeletal skinning, CPU/GPU evaluation |
| **Instancing** | 5 | 🚀 Future | Instance buffers, batching by (mesh, material) |
| **Indirect Buffers** | 5 | 🚀 Future | GPU-side draw parameters, GPU-driven culling |
| **ImGui** | Future | 🚀 Future | UI overlay, debug tools |
| **Async Logging** | Future | 🚀 Future | Non-blocking log I/O |
| **CI** | Future | 🚀 Future | GitHub Actions, multi-platform builds |
| **Profiling** | Future | 🚀 Future | Tracy, RenderDoc integration |

---

## Current Execution Plan

**For next phase (Phase 3):**

1. Review current material binding in shaders
2. Implement per-material texture binding (descriptor sets already in place)
3. Add UBO for per-frame/per-object data (MVP, colors, etc.)
4. Load material properties from glTF or config
5. Test: Scene with multiple textured meshes, different materials

**Parallel tracks (as time allows):**
- Animation M1 (parsing, logging) — important for glTF support
- Optimization investigation (profile before instancing to know if needed)

---

## Architecture & Related Docs

See [architecture.md](architecture.md) for the full module layout, init/cleanup order, and design patterns.

| Document | Link | Purpose |
|----------|------|---------|
| **Module Reference** | [architecture.md](architecture.md) | Vulkan stack, managers, resource loading lifecycle, async cleanup, threading |
| **System Design** | [engine-design.md](engine-design.md) | GameObject/Component, Physics, Scripting, memory layout, gap analysis |
| **Getting Started** | [getting-started.md](getting-started.md) | Build, run, config files |
| **Troubleshooting** | [troubleshooting.md](troubleshooting.md) | Common issues and fixes |
| **Guidelines** | [guidelines/coding-guidelines.md](guidelines/coding-guidelines.md) | Code style, patterns |
| **Vulkan Details** | [vulkan/](vulkan/) | Validation layers, swapchain rebuild, version support |
| **Platforms** | [platforms/](platforms/) | macOS, iOS, Android notes |
| **Animation Roadmap** | [future-ideas/animation-skinning-roadmap.md](future-ideas/animation-skinning-roadmap.md) | Detailed animation+skinning plan |
| **Indirect Buffers** | [future-ideas/indirect-buffers.md](future-ideas/indirect-buffers.md) | When and how to use indirect draws |

---

## TODOs and Known Issues

**Code-level TODOs** (logged as warnings in code):
- Animation import (SceneManager logs warning when glTF clip is loaded)
- Skinning import (SceneManager logs warning when glTF skin is loaded)
- Shader cache across swapchain recreate (vulkan_app.cpp)

**See also:** [future-ideas/animation-skinning-roadmap.md](future-ideas/animation-skinning-roadmap.md) for the animation/skinning implementation plan.

---

## Development Workflow

1. **Setup:** Run platform setup script (scripts/windows/setup_windows.bat, etc.)
2. **Build:** Run build script (scripts/windows/build.bat --debug, etc.)
3. **Run:** ./install/[Debug|Release]/bin/VulkanApp levels/default/level.json
4. **Edit:** Modify source, rebuild; configs are hotloadable
5. **Debug:** Use validation layers (Debug build only), RenderDoc, etc.

See [getting-started.md](getting-started.md) for details.
