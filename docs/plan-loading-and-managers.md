# Plan: Loading and Managers

This document describes the roadmap for the generic loader/job system and the managers module (pipeline, mesh, texture). Objects (e.g. cubes) and draw logic are not implemented yet.

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
- **Pipeline manager**: get-or-create pipeline by (vert path, frag path). Depends on shader manager. Used by drawables (e.g. cubes).
- **Shader manager**: lives in `vulkan/`; used by pipeline manager. No move.
- **Mesh manager** (future): get-or-load mesh by path. Consumes LoadMesh results; creates vertex/index buffers on main thread. Cubes and terrain depend on it.
- **Texture manager** (future): get-or-load texture by path. Consumes LoadTexture results; creates image + view + sampler on main thread.

**Dependency chain**: Shaders → Pipeline → Drawable (Pipeline + Mesh (+ Texture)). Multiple cubes = one pipeline, one mesh asset, N instances.

---

## 3. Not done yet

- No implementation of cubes, meshes, or terrain.
- No vertex buffers or draw calls for objects.
- No new job types beyond the refactor (LoadFile + reserved types).
- No pipeline manager logic beyond stub/comment.
- No mesh/texture manager logic beyond comments/stub.

---

## 4. Order of work

1. **Done**: Plan document; managers module scaffold (comments + one or two minimal pieces).
2. **Done**: Refactor job queue to generic typed job system (`LoadJobType`, `CompletedLoadJob`, `ProcessCompletedJobs(handler)`); worker pushes to completed queue; main thread drains each frame with placeholder handler; shader loading unchanged (blocking GetShader).
3. **Later**: Implement pipeline manager (get-or-create by vert+frag).
4. **Later**: Mesh loading job type + mesh manager; then cube drawables.
