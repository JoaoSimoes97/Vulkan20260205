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
| **Material** | How to draw: pipeline key + pipeline layout descriptor (push ranges; later descriptor set layouts). Objects reference material by id. | Material table / registry + PipelineManager |
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

### Phase 1.5: Depth and multi-viewport prep (before Phase 2)

Done **before** Phase 2 so the recording path (render pass, framebuffers, Record) is stable. Phase 2 then only adds scene/mesh/material and plugs into the same target API.

- **Render pass descriptor**: Create from a descriptor (color format + optional depth format, load/store ops). Build attachments and subpass from that; `pDepthStencilAttachment` set when depth format is valid.
- **Framebuffers**: Create from **list of attachment views** (color + optional depth) + extent. So main pass uses swapchain views + depth view; later, offscreen viewports use own image views.
- **Depth image**: New helper or small module: create depth image + view (format, extent); recreate with extent; pass view into framebuffer creation.
- **Pipeline depth state**: Add to `GraphicsPipelineParams` (or pipeline descriptor): `depthTestEnable`, `depthWriteEnable`, `depthCompareOp`. In `VulkanPipeline::Create`, build `VkPipelineDepthStencilStateCreateInfo` when depth is enabled; otherwise keep `pDepthStencilState = nullptr`.
- **Record refactor**: Take **render area** (offset + extent), **viewport** (full struct), **scissor** (or derive from render area), and **clear values** as **array + count** (color + depth when applicable). One API works for fullscreen and for a region, and for color-only or color+depth. Enables multiple viewports (e.g. ImGui) later without further refactor.
- **App**: Use new API for main pass (fullscreen, with depth). Draw list source unchanged (e.g. `m_objects`); only the Record call site and render pass/framebuffer creation change.

**Deliverable**: Depth buffer in use for main pass; Record and render pass/framebuffers parameterized so multiple viewports (e.g. ImGui) are easy to add later.

---

### Phase 2: MeshManager and material notion

- **MeshManager**: Load one mesh (file or procedural); return opaque `MeshId` and, when needed, fill draw params (vertex count, first vertex, index count, etc.). Owns vertex/index buffers.
- **Material**: Simple table or struct: material id → pipeline key + layout descriptor. Used when building the draw list to resolve pipeline and layout.
- **Scene**: List of renderables. Each entry: mesh id, material id, per-object data (e.g. transform mat4 or small blob). Editor/app adds, removes, updates objects.
- **RenderListBuilder**: Input = scene + PipelineManager + MeshManager. Output = `std::vector<DrawCall>`. For each object: resolve pipeline/layout via material id, resolve mesh draw params, push object data into DrawCall, append. Sort by pipeline to minimize state changes. Reuse one vector per frame (clear and fill).
- **App**: Create a few objects (different mesh/material/data), build draw list via RenderListBuilder, `DrawFrame(drawCalls)`. Single render pass; one draw per object for now.

**Deliverable**: Scene-driven rendering; many objects with different meshes and materials; draw list built in one place.

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

- [ ] Sort draw list by (pipeline, mesh) to minimize pipeline and vertex buffer binds.
- [ ] Reuse single `std::vector<DrawCall>` each frame; avoid per-frame allocations in build path.
- [ ] Push constant size matches layout (parameterized); no oversized pushes.
- [ ] Instancing: one draw per (mesh, material) group when multiple objects share both.
- [ ] Frustum culling (optional in editor): skip objects outside camera in RenderListBuilder.
- [ ] Descriptor sets: shared per material or pooled; bindless only if needed later.

---

## Module summary

| Module | Role |
|--------|------|
| **PipelineManager** | Get pipeline and layout by key; layout descriptor per key; creates pipelines with different push (and later set) layouts. |
| **MeshManager** | Load meshes; return MeshId and draw params; own vertex/index buffers. |
| **Scene / ObjectManager** | List of renderables (mesh id, material id, per-object data). API-agnostic. |
| **RenderListBuilder** | Scene + PipelineManager + MeshManager → `std::vector<DrawCall>`; sort; optional instancing. |
| **VulkanCommandBuffers** | Record(target: render area, viewport, scissor, clear values; drawCalls). No knowledge of objects or scene. After Phase 1.5: supports depth clear and multi-viewport-ready. |

---

## Related docs

| Doc | Purpose |
|-----|---------|
| [architecture.md](architecture.md) | Module layout, rendering and draw list section. |
| [plan-rendering-and-materials.md](plan-rendering-and-materials.md) | Pipeline layout param, blend, materials. |
| [plan-loading-and-managers.md](plan-loading-and-managers.md) | Job system, MeshManager placement. |
