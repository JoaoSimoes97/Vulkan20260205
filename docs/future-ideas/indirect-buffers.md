# Indirect buffers (future)

Indirect draw and dispatch in Vulkan: what they are, when they help performance, and when to add them.

---

## What they are

**Normal draw:** The CPU records `vkCmdDraw(vertexCount, instanceCount, firstVertex, firstInstance)` (or `vkCmdDrawIndexed`). The GPU runs one draw with those arguments.

**Indirect draw:** The CPU does not pass the draw arguments directly. Instead:

1. **You fill a buffer** (in GPU-visible memory) with the same kind of arguments (vertex count, instance count, first vertex, etc.), one struct per draw.
2. **You record** a single command: `vkCmdDrawIndirect(buffer, offset, drawCount, stride)` (or `vkCmdDrawIndexedIndirect`).
3. **The GPU reads** that buffer and executes one draw per “record” in the buffer.

So the **indirect buffer** is just a buffer of draw (or dispatch) parameters; the GPU reads it instead of the driver passing parameters from the CPU.

---

## Do they improve performance?

**Short answer:** They can improve performance in specific situations (fewer CPU→GPU round-trips, batching, GPU-driven culling). They do **not** automatically make every app faster.

### 1. Fewer CPU submissions (batch many draws)

- **Without indirect:** For 10,000 draws you record 10,000 `vkCmdDraw*` in the command buffer (or split across multiple submits). Each draw is one command; the CPU built 10,000 commands.
- **With indirect (multi-draw):** You fill a buffer with 10,000 “draw argument” structs (one per draw), then call **one** `vkCmdDrawIndirect` with `drawCount = 10000`. The GPU reads all 10,000 entries and does 10,000 draws.

**Performance:** You issue one indirect draw command instead of 10,000 direct draws. That reduces CPU overhead and submission cost. The actual draw work (triangles, state changes) is the same; gains are on **CPU** and **command throughput**, not on raw rasterization.

### 2. GPU-driven pipelines (advanced)

You can go further and **generate the indirect buffer on the GPU** (e.g. in a compute shader): culling, LOD, occlusion. The CPU does not even know how many draws will run; the GPU decides and writes the indirect buffer, then you do one `vkCmdDrawIndirect` (or multi-draw indirect).

**Performance:** Removes CPU work for culling/LOD; can reduce draws (cull off-screen, etc.) and improve GPU utilization. More complex (compute pass, sync, debugging).

### 3. When they don’t help much

- **Few draws (e.g. tens of draws):** Overhead of filling and managing the indirect buffer can outweigh the benefit. Direct `vkCmdDraw*` is simpler and often enough.
- **Heavy per-draw CPU work anyway:** If you still do a lot of work per object on the CPU (complex logic, many state changes), indirect does not remove that; it only changes how draw arguments are passed.
- **Instancing already in use:** If you already batch with instancing (one draw, many instances), you’ve already reduced draw count; indirect adds another layer of batching on top and may not be worth it until you have a huge number of “batch” draws.

---

## Summary

| Aspect | Explanation |
|--------|-------------|
| **What** | A buffer in GPU memory holding draw (or dispatch) parameters; you call `vkCmdDrawIndirect` and the GPU reads that buffer to know what to draw. |
| **Performance** | **Yes, in the right cases:** fewer draw commands and less CPU work when you have many draws, and potential for GPU-driven culling. **No magic:** same raster work; gains are on CPU and command throughput. |
| **When to consider** | Many draws (hundreds/thousands+), or when you want GPU-driven culling/LOD. After instancing and descriptor sets, indirect is the next step if you’re still CPU-bound on draw count. |

---

## Order of work

Do **descriptor sets** and **materials + textures** first, then **instancing**. Add **indirect buffers** only if you still need to reduce CPU cost from a very high draw count or want a GPU-driven pipeline.
