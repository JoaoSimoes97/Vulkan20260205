# Plan: Rendering and materials

Roadmap for the draw loop, pipeline state (including blend), and materials. Complements [plan-loading-and-managers.md](plan-loading-and-managers.md) (shader/pipeline/mesh/texture loading) and [architecture.md](architecture.md) (module layout).

---

## Current state

- **Done**: Swapchain, render pass, framebuffers, pipeline with `GraphicsPipelineParams` (topology, rasterization, MSAA), pipeline manager (get by key, params at get time). Draw loop: acquire → record (render pass + list of `DrawCall`s) → submit → present; out-of-date handling. Command buffers record a **draw list**: each `DrawCall` has pipeline, layout, optional push constants, and draw params; app builds the list each frame (e.g. one draw per object). Orthographic aspect-correct projection (mat4) pushed per draw so the triangle is not stretched on non-square windows.
- **Hardcoded**: Pipeline layout is a single push constant range of 64 bytes (vertex stage) inside `VulkanPipeline::Create`. Blend off; no material concept; no vertex buffers or mesh manager yet.

---

## 1. Draw loop — done

The frame path is implemented: acquire image, record (render pass + list of `DrawCall`s: bind pipeline, push constants, draw per item), submit, present. Resize and out-of-date handling call `RecreateSwapchainAndDependents()`. See [vulkan/tutorial-order.md](vulkan/tutorial-order.md) and [vulkan/swapchain-rebuild-cases.md](vulkan/swapchain-rebuild-cases.md).

---

## 2. Pipeline layout parameterization (next)

Make pipeline layout driven by a descriptor so different pipelines can have different push constant sizes/stages (and later descriptor set layouts).

| Step | What |
|------|------|
| Layout descriptor | Introduce a small struct (e.g. `PipelineLayoutDescriptor`) holding `std::vector<VkPushConstantRange>` (and later set layouts). |
| VulkanPipeline::Create | Build `VkPipelineLayoutCreateInfo` from this descriptor instead of hardcoded 64-byte vertex range. |
| PipelineManager | Store layout descriptor per key; pass it when creating pipelines. Cache invalidation must include layout (e.g. compare push ranges) so layout change triggers rebuild. |
| DrawCall / recording | Unchanged: one push blob per draw; app (or RenderListBuilder) ensures size matches the pipeline’s layout. |

**Goal**: Support multiple pipelines with different push constant layouts (e.g. main = mat4; UI = mat4 + color; some = no push). Required for editor with many objects and different materials.

See [plan-editor-and-scene.md](plan-editor-and-scene.md) for the full editor/scene plan.

---

## 3. Pipeline params — blend (transparency)

Add blend state to **GraphicsPipelineParams** so opaque vs transparent is caller-driven like other pipeline options.

| Step | What |
|------|------|
| Params | Add e.g. `blendEnable`, `srcColorBlendFactor`, `dstColorBlendFactor`, `colorBlendOp` (and alpha factors/op if needed). |
| Pipeline | In `VulkanPipeline::Create`, build `VkPipelineColorBlendAttachmentState` (and attachment count) from `pipelineParams`. |
| Caller | Fills params (opaque vs alpha blend); manager already recreates when params change. |

**Goal**: Support opaque and alpha-blend pipelines via params only; no second pipeline API.

---

## 4. Scene, objects, and draw list (editor)

For an editor with many objects and different GPU data: introduce **Scene** (list of renderables: mesh id, material id, per-object data), **MeshManager** (load meshes, return draw params), and **RenderListBuilder** (build `std::vector<DrawCall>` from scene, sort by pipeline, optional instancing). Material = pipeline key + layout; objects reference material id. See [plan-editor-and-scene.md](plan-editor-and-scene.md) for phased implementation and optimization.

---

## 5. Materials (later)

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

## 6. After materials (order flexible)

- **Vertex buffers** and vertex input in pipeline.
- **Depth buffer** (attachment + pipeline depth state).
- **Uniform buffers** (e.g. per-frame, per-object) and **pipeline layout** (descriptor set layouts, push constants).
- **Textures** (descriptor sets: image view + sampler).
- Optional: **Shader cache across swapchain recreate** (see TODO in `vulkan_app.cpp`).

See [future-ideas/README.md](future-ideas/README.md) (Rendering) for more.

---

## 7. Out of scope for this plan

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
