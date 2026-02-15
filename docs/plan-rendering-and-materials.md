# Plan: Rendering and materials

Roadmap for the draw loop, pipeline state (including blend), and materials. Complements [plan-loading-and-managers.md](plan-loading-and-managers.md) (shader/pipeline/mesh/texture loading) and [architecture.md](architecture.md) (module layout).

---

## Current state

- **Done**: Swapchain, render pass (from descriptor: color + optional depth), **VulkanDepthImage**, framebuffers (color + optional depth views), pipeline with `GraphicsPipelineParams` (including **depth state**, **blend state**, and vertex input), pipeline manager (get by key, returns VkPipeline/VkPipelineLayout; shaders via shared_ptr + deleter, TrimUnused). **Pipeline layout parameterization**: `PipelineLayoutDescriptor` (push ranges). Draw loop: acquire → record (render area, viewport, scissor, clear values, draw list; **vertex buffer bind per draw**) → submit → present; out-of-date handling. **DrawCall** includes vertex buffer + offset; command buffers bind vertex buffers then draw. **MeshManager** (MeshHandle owns VkBuffer/VkDeviceMemory), **MaterialManager**, **TextureManager** (TextureHandle: image/view/sampler; stb_image, TrimUnused), **Scene**/SceneManager, **RenderListBuilder** (build draw list from scene, sort by pipeline/mesh; optional frustum culling and push constant size validation). Perspective and orthographic projection (config-driven); view matrix from camera position; initial camera and pan speed from config. Resize syncs swapchain to drawable size every frame. **Single Cleanup() path**: UnloadScene → MeshManager.Destroy() → TextureManager.Destroy() → ShaderManager.Destroy() → Device.
- **Not yet**: Materials + textures (bind texture to descriptor set, add set layout to pipeline, bind set in Record); instancing. Descriptor set infrastructure (layout, pool, one set) is in place.

---

## 1. Draw loop — done

The frame path is implemented: acquire image, record (render pass + list of `DrawCall`s: bind pipeline, bind vertex buffer, push constants, draw per item), submit, present. Resize and out-of-date handling call `RecreateSwapchainAndDependents()`. See [vulkan/tutorial-order.md](vulkan/tutorial-order.md) and [vulkan/swapchain-rebuild-cases.md](vulkan/swapchain-rebuild-cases.md).

---

## 2. Pipeline layout parameterization — done

Pipeline layout is driven by `PipelineLayoutDescriptor` (push ranges); PipelineManager stores it per key. See [plan-editor-and-scene.md](plan-editor-and-scene.md) Phase 1.

---

## 3. Depth and multi-viewport prep — done

Render pass descriptor (color + optional depth), framebuffers with attachment list, VulkanDepthImage, pipeline depth state, and Record(render area, viewport, scissor, clear value array). The recording path is stable and multi-viewport-ready. Enables depth for 3D and prepares for multiple viewports (e.g. ImGui). See [plan-editor-and-scene.md](plan-editor-and-scene.md) Phase 1.5.

---

## 4. Pipeline params — blend (transparency) — done

Blend state is in **GraphicsPipelineParams** (`blendEnable`, `srcColorBlendFactor`, `dstColorBlendFactor`, `colorBlendOp`, and alpha factors/op). **VulkanPipeline::Create** builds `VkPipelineColorBlendAttachmentState` from `pipelineParams`. Caller sets params (opaque vs alpha blend); manager recreates when params change. Opaque and alpha-blend pipelines via params only; no second pipeline API.

---

## 5. Scene, objects, and draw list (editor)

**Implemented**: **Scene** (list of renderables: mesh + material refs, per-object data), **MeshManager** (vertex buffer upload, MeshHandle owns buffers), **RenderListBuilder** (build `std::vector<DrawCall>` from scene, vertex buffer + draw params per mesh, sort by pipeline/mesh; frustum culling when viewProj passed; push constant size validated). Material = pipeline key + layout; objects hold `shared_ptr<MaterialHandle>` and `shared_ptr<MeshHandle>`. See [plan-editor-and-scene.md](plan-editor-and-scene.md). **Not yet**: descriptor sets (textures in shaders), instancing.

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
- **Depth** is implemented (Phase 1.5).
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
