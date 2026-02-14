# Plan: Rendering and materials

Roadmap for the draw loop, pipeline state (including blend), and materials. Complements [plan-loading-and-managers.md](plan-loading-and-managers.md) (shader/pipeline/mesh/texture loading) and [architecture.md](architecture.md) (module layout).

---

## Current state

- **Done**: Swapchain, render pass, framebuffers, pipeline with `GraphicsPipelineParams`, pipeline manager (get by key, layout descriptor per key). **Pipeline layout parameterization**: `PipelineLayoutDescriptor` (push ranges); different pipelines can have different push layouts. Draw loop: acquire → record (render pass + list of `DrawCall`s) → submit → present; out-of-date handling. Command buffers record a **draw list**; app builds the list each frame from objects (e.g. `m_objects`). Orthographic aspect-correct projection (mat4) + per-object color (vec4) pushed per draw. Simple **Object** class (Shape, localTransform, color, pushData) and debug shapes (triangle, circle, rectangle, cube).
- **Not yet**: Depth buffer; blend; render pass/framebuffer/Record parameterized for multi-viewport; vertex buffers; MeshManager; material concept.

---

## 1. Draw loop — done

The frame path is implemented: acquire image, record (render pass + list of `DrawCall`s: bind pipeline, push constants, draw per item), submit, present. Resize and out-of-date handling call `RecreateSwapchainAndDependents()`. See [vulkan/tutorial-order.md](vulkan/tutorial-order.md) and [vulkan/swapchain-rebuild-cases.md](vulkan/swapchain-rebuild-cases.md).

---

## 2. Pipeline layout parameterization — done

Pipeline layout is driven by `PipelineLayoutDescriptor` (push ranges); PipelineManager stores it per key. See [plan-editor-and-scene.md](plan-editor-and-scene.md) Phase 1.

---

## 3. Depth and multi-viewport prep (next)

Render pass descriptor (color + optional depth), framebuffers with attachment list, depth image, pipeline depth state, and Record(render area, viewport, scissor, clear value array). Done **before** Phase 2 (MeshManager, Scene, RenderListBuilder) so the recording path is stable. Enables depth for 3D and prepares for multiple viewports (e.g. ImGui). See [plan-editor-and-scene.md](plan-editor-and-scene.md) Phase 1.5.

---

## 4. Pipeline params — blend (transparency)

Add blend state to **GraphicsPipelineParams** so opaque vs transparent is caller-driven like other pipeline options.

| Step | What |
|------|------|
| Params | Add e.g. `blendEnable`, `srcColorBlendFactor`, `dstColorBlendFactor`, `colorBlendOp` (and alpha factors/op if needed). |
| Pipeline | In `VulkanPipeline::Create`, build `VkPipelineColorBlendAttachmentState` (and attachment count) from `pipelineParams`. |
| Caller | Fills params (opaque vs alpha blend); manager already recreates when params change. |

**Goal**: Support opaque and alpha-blend pipelines via params only; no second pipeline API.

---

## 5. Scene, objects, and draw list (editor)

For an editor with many objects and different GPU data: introduce **Scene** (list of renderables: mesh id, material id, per-object data), **MeshManager** (load meshes, return draw params), and **RenderListBuilder** (build `std::vector<DrawCall>` from scene, sort by pipeline, optional instancing). Material = pipeline key + layout; objects reference material id. See [plan-editor-and-scene.md](plan-editor-and-scene.md) for phased implementation and optimization.

---

## 6. Materials (later)

**Material** = appearance (albedo, roughness, metallic, etc.) + rendering state (blend, depth, cull).

| Aspect | Where it goes |
|--------|----------------|
| **Rendering state** | Drives `GraphicsPipelineParams` → pipeline choice (existing: GetPipelineIfReady(..., pipelineParams)). |
| **Appearance (scalars/vectors)** | UBOs or push constants; shaders read them; material is the data in that UBO (or the source that fills it). |
| **Textures** | Descriptor sets (image view + sampler); material = which descriptor set / which textures you bind before the draw. |
| **Flow** | Per object/draw: get material → pipeline from state → bind descriptors (UBO + textures) → draw. |
| **Where configured** | Per draw (code) or material table/config (id → pipeline params + descriptor data); caller remains source of truth for params. |

**Multiple pipelines** → multiple draw calls (at least one per pipeline); batch by pipeline to minimize binds.

---

## 7. After materials (order flexible)

- **Vertex buffers** and vertex input in pipeline.
- **Depth** is covered in Phase 1.5 (depth and multi-viewport prep).
- **Uniform buffers** (e.g. per-frame, per-object) and **pipeline layout** (descriptor set layouts, push constants).
- **Textures** (descriptor sets: image view + sampler).
- Optional: **Shader cache across swapchain recreate** (see TODO in `vulkan_app.cpp`).

See [future-ideas/README.md](future-ideas/README.md) (Rendering) for more.

---

## 8. Out of scope for this plan

- Multiple cameras, shadow maps, raytracing — see [architecture.md](architecture.md) (Future extensions).
- Async logging, CI, ImGui — see [future-ideas/README.md](future-ideas/README.md).

---

## Related docs

| Doc | Purpose |
|-----|---------|
| [architecture.md](architecture.md) | Module layout, init/cleanup, swapchain, config. |
| [plan-loading-and-managers.md](plan-loading-and-managers.md) | Loader/job system, pipeline/mesh/texture managers (pipeline manager done). |
| [vulkan/tutorial-order.md](vulkan/tutorial-order.md) | Tutorial-style init/draw/recreate checklist. |
| [vulkan/swapchain-rebuild-cases.md](vulkan/swapchain-rebuild-cases.md) | When to rebuild swapchain and how the loop reacts. |
| [future-ideas/README.md](future-ideas/README.md) | Logging, build, rendering (vertex buffers, depth, UBOs, textures), ImGui, etc. |
