# Plan: Editor and scene (many objects, different GPU data)

Roadmap for supporting an editor where users load many objects, each with different data passed to the GPU. Complements [architecture.md](architecture.md), [plan-rendering-and-materials.md](plan-rendering-and-materials.md), and [plan-loading-and-managers.md](plan-loading-and-managers.md).

---

## Goals

- **Editor**: User can load many objects; each object can have different mesh, material, and per-object GPU data (transform, color, etc.).
- **Organized**: Clear split between scene (what to draw), assets (meshes, materials), pipelines (how to draw), and draw list building (connection).
- **Optimized**: Sort by pipeline (minimize state changes), optional instancing, reusable draw list buffer, no per-frame allocations in the hot path.

---

## Concepts

| Concept | Responsibility | Owned by |
|--------|----------------|----------|
| **Mesh** | Geometry: vertex/index buffers, draw params (vertex count, first vertex, index count). Loaded once, shared by many objects. | MeshManager |
| **Material** | How to draw: pipeline key + layout + rendering state (blend, cull, depth). Resolves to `VkPipeline`/`VkPipelineLayout` via PipelineManager. Objects reference material by id. **MaterialManager** holds registry; TrimUnused() drops materials no object uses. | MaterialManager |
| **Renderable / Object** | One drawable: mesh id, material id, per-object GPU data (e.g. transform mat4, color). | Scene / ObjectManager |
| **Draw list** | Built each frame from scene: resolve pipeline and mesh params, sort, optionally batch/instance, produce `std::vector<DrawCall>`. | RenderListBuilder |

Pipeline layout definition stays in pipeline land; the scene only holds references (material id) and the data to push.

---

## Phased implementation

### Phase 1: Pipeline layout parameterization — done

- **PipelineLayoutDescriptor** in `vulkan_pipeline.h`: `std::vector<VkPushConstantRange>`, with `operator==` for cache comparison.
- **VulkanPipeline::Create** takes `const PipelineLayoutDescriptor&`; builds `VkPipelineLayout` from it (empty ranges = no push constants).
- **PipelineManager**: `GetPipelineIfReady(..., layoutDescriptor)`; stores `lastLayout` per entry; recreates pipeline when layout changes.
- **App**: Passes `mainLayoutDesc` (one range, 64 bytes, vertex) for the "main" pipeline.

**Deliverable**: Different pipelines can have different push constant sizes and stages without code changes.

---

### Phase 1.5: Depth and multi-viewport prep — done

The recording path (render pass, framebuffers, Record) is stable. Phase 2 only adds scene/mesh/material and plugs into the same target API.

**Current implementation:**
- **Render pass**: Created from `RenderPassDescriptor` (color + optional depth format, load/store ops). `HasDepthAttachment()` used for clear value count and pipeline.
- **VulkanDepthImage**: Create/destroy from device, physical device, format, extent; `FindSupportedFormat` for format selection. Recreated with swapchain.
- **Framebuffers**: Created from list of attachment views (swapchain + optional depth view) + extent.
- **Pipeline depth state**: In `GraphicsPipelineParams`; `VulkanPipeline::Create` builds depth stencil state when render pass has depth.
- **Record**: `Record(index, renderPass, framebuffer, renderArea, viewport, scissor, drawCalls, pClearValues, clearValueCount)`. Multi-viewport-ready.
- **App**: Main pass uses fullscreen extent, viewport, scissor, color + depth clear; build draw list from `m_objects`, then `DrawFrame(drawCalls)`.

---

### Phase 2: MaterialManager, MeshManager, Scene, RenderListBuilder

- **MaterialManager** (implemented): Registry material id → `shared_ptr<MaterialHandle>`. Material caches `shared_ptr<PipelineHandle>`; resolves to `VkPipeline`/`VkPipelineLayout`. TrimUnused() when no object uses a material. See [plan-loading-and-managers.md](plan-loading-and-managers.md) §0.
- **MeshManager** (implemented): **MeshHandle** (class) owns `VkBuffer` and `VkDeviceMemory`. Get-or-create procedural by key (triangle, circle, rectangle, cube) with vertex buffer upload; RequestLoadMesh + OnCompletedMeshFile parse .obj and upload vertex buffer. Return `shared_ptr<MeshHandle>`. **Destroy()** clears cache (call before device destroy); call **UnloadScene()** first so no scene refs keep handles alive. TrimUnused() when no object uses mesh.
- **Scene** (implemented): Container: `std::vector<Object>`, name, **Clear()**. Data only; no Vulkan. Objects hold `shared_ptr<MaterialHandle>` and `shared_ptr<MeshHandle>`; when scene is unloaded or objects removed, refs drop and managers can TrimUnused().
- **SceneManager** (implemented): Owns current scene. **LoadSceneAsync(path)** — file load via JobQueue, parse JSON on main when completed; **UnloadScene()**, **SetCurrentScene()**, **CreateDefaultScene()**; **AddObject** / **RemoveObject**. SetDependencies(JobQueue, MaterialManager, MeshManager). ProcessPendingDestroys() on pipelines at start of frame (after `vkWaitForFences`).
- **Draw list**: Built each frame from current scene: for each object with valid material and mesh (HasValidBuffer()), material → pipeline/layout, mesh → vertex buffer, offset, and draw params; fill DrawCall; sort by (pipeline, vertex buffer, …); Record binds vertex buffers before each draw. TrimUnused() on shader/pipeline/material/mesh once per frame.
- **App**: Holds SceneManager; sets default scene (CreateDefaultScene) or calls LoadSceneAsync; build draw list from GetCurrentScene(); DrawFrame(drawCalls).

**Deliverable**: Scene-driven rendering; many objects with different meshes and materials; draw list built in one place; resources released when unused.

---

### Phase 3: Editor loading and many objects

- Editor (or app) loads multiple meshes via MeshManager; registers multiple materials (pipeline key + layout per material).
- Create many objects: assign mesh id, material id, per-object data (transform, etc.).
- Confirm draw list scales (many DrawCalls), all objects render correctly, and pipeline sorting reduces binds.

**Deliverable**: Editor can load many objects with different GPU data; no new managers, just scaling the existing scene and build path.

---

### Phase 4: Instancing and batching

- In **RenderListBuilder**: Group objects that share (mesh, material). Emit one `DrawCall` per group with `instanceCount > 1` and per-instance data from a small buffer or second push range (or instance vertex buffer).
- Reduces draw calls when many objects share mesh and material.

**Deliverable**: Fewer draw calls for repeated objects; better GPU utilization.

---

### Phase 5: Descriptors and textures (later)

- Add descriptor set layouts to **PipelineLayoutDescriptor**; materials can reference textures/UBOs.
- Descriptor set strategy: shared sets per material or small pool; avoid one set per object at scale.
- Objects still supply per-instance push data (or indices into shared data).

---

## Optimization checklist

- [x] Sort draw list by (pipeline, mesh) to minimize pipeline and vertex buffer binds (RenderListBuilder).
- [x] Reuse single `std::vector<DrawCall>` each frame; avoid per-frame allocations in build path (VulkanApp::m_drawCalls).
- [x] Push constant size validated against material layout; oversized pushes skipped in RenderListBuilder.
- [ ] Instancing: one draw per (mesh, material) group when multiple objects share both.
- [x] Frustum culling: optional in RenderListBuilder when viewProj passed; objects outside NDC skipped.
- [ ] Descriptor sets: shared per material or pooled; bindless only if needed later.

---

## Module summary

| Module | Role |
|--------|------|
| **PipelineManager** | Get VkPipeline/VkPipelineLayout by key; DestroyPipelines() on swapchain recreate. |
| **MaterialManager** | Registry material id → `shared_ptr<MaterialHandle>`; resolve to VkPipeline/VkPipelineLayout; TrimUnused(). |
| **MeshManager** | MeshHandle owns VkBuffer/VkDeviceMemory. Procedural by key + RequestLoadMesh/OnCompletedMeshFile; vertex buffer upload implemented. SetDevice/SetPhysicalDevice/SetQueue/SetQueueFamilyIndex before use. Destroy() clears cache (before device); UnloadScene() before Destroy(). TrimUnused(). |
| **Scene** | Container: objects, name; Clear() drops refs. |
| **SceneManager** | Current scene; LoadSceneAsync(path), UnloadScene, SetCurrentScene, CreateDefaultScene, AddObject/RemoveObject. Async load via JobQueue; parse JSON when file ready. |
| **RenderListBuilder** | Scene + managers → `std::vector<DrawCall>`; sort by (pipeline, mesh). Optional instancing later. |
| **VulkanCommandBuffers** | Record(render area, viewport, scissor, drawCalls, clear values). Binds vertex buffer per draw from DrawCall. No knowledge of objects or scene. Supports depth clear and multi-viewport-ready. |

---

## Related docs

| Doc | Purpose |
|-----|---------|
| [architecture.md](architecture.md) | Module layout, rendering and draw list section. |
| [plan-rendering-and-materials.md](plan-rendering-and-materials.md) | Pipeline layout param, blend, materials. |
| [plan-loading-and-managers.md](plan-loading-and-managers.md) | Job system, MeshManager placement. |
