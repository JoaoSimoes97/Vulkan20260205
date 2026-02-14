# Plan: Loading and Managers

This document describes the roadmap for the generic loader/job system and the managers module (pipeline, mesh, texture). For the draw loop, blend params, and materials see [plan-rendering-and-materials.md](plan-rendering-and-materials.md).

---

## 1. Generic loader / job system

- **Refactor JobQueue** into a typed job system:
  - Job types: at least `LoadFile` (path → bytes); reserve `LoadMesh`, `LoadTexture` for later.
  - One worker loop executes the right logic per type; completed results go into a “completed” queue with type + data.
- **Main thread**: once per frame (or after submit), drain the completed queue and **dispatch by type** (e.g. File → shader manager; later Mesh → mesh manager; Texture → texture manager).
- **Shader manager** continues to work as today but consumes “file loaded” results from this generic system.

---

## 2. Managers module (scaffold)

- **Location**: `src/managers/`.
- **Pipeline manager**: get-or-create pipeline by key (vert path, frag path); caller passes `GraphicsPipelineParams` at get time (params at get time, single source of truth). Depends on shader manager. **Done.** Used by drawables (e.g. cubes).
- **Shader manager**: lives in `vulkan/`; used by pipeline manager. No move.
- **Mesh manager** (next for editor): get-or-load mesh by path (or create procedurally). Returns mesh id and draw params; creates vertex/index buffers on main thread. Used by Scene and RenderListBuilder so the editor can load many objects. See [plan-editor-and-scene.md](plan-editor-and-scene.md).
- **Texture manager** (future): get-or-load texture by path. Consumes LoadTexture results; creates image + view + sampler on main thread.

**Dependency chain**: Shaders → Pipeline → Material (pipeline key + layout). Scene objects reference mesh id + material id; RenderListBuilder turns scene + PipelineManager + MeshManager into draw list. Multiple objects = many DrawCalls, optionally batched by (mesh, material) for instancing.

---

## 3. Editor and many objects

To support an editor with many objects and different GPU data per object: MeshManager provides mesh ids and draw params; Scene holds renderables (mesh id, material id, per-object data); RenderListBuilder produces the draw list from scene + PipelineManager + MeshManager. Pipeline layout is parameterized per pipeline key so different materials can have different push constant layouts. See [plan-editor-and-scene.md](plan-editor-and-scene.md) for the full phased plan.

---

## 4. Not done yet

- No MeshManager implementation; no vertex buffers or mesh loading.
- No Scene, RenderListBuilder, or material table (plan in [plan-editor-and-scene.md](plan-editor-and-scene.md)).
- No new job types beyond the refactor (LoadFile + reserved types).
- No texture manager logic beyond stub.

---

## 5. Order of work

1. **Done**: Plan document; managers module scaffold.
2. **Done**: Refactor job queue to generic typed job system (`LoadJobType`, `CompletedLoadJob`, `ProcessCompletedJobs(handler)`); worker pushes to completed queue; main thread drains each frame; shader loading unchanged (blocking GetShader).
3. **Done**: Pipeline manager (get-or-create by key, params at get time, ref-counted shaders).
4. **Done**: Phase 1.5 (depth and multi-viewport prep): VulkanDepthImage, render pass/framebuffers/Record parameterized, pipeline depth state. See [plan-editor-and-scene.md](plan-editor-and-scene.md).
5. **Next**: Phase 2 (MeshManager, Scene, RenderListBuilder). See [plan-editor-and-scene.md](plan-editor-and-scene.md) and [plan-rendering-and-materials.md](plan-rendering-and-materials.md).
