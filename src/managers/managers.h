#pragma once

/*
 * Managers module: pipeline, mesh, texture (and shader manager in vulkan/).
 * See src/managers/README.md and docs/plan-loading-and-managers.md.
 *
 * PipelineManager: request pipelines by key; non-blocking. GetPipelineIfReady(key, ..., layoutDescriptor) when shaders ready; layout per key.
 * MeshManager:     (future) get-or-load mesh by path; vertex/index buffers on main thread.
 * TextureManager:  (future) get-or-load texture by path; VkImage + view + sampler on main thread.
 *
 * Dependency: Shaders -> Pipeline -> Drawable (Pipeline + Mesh + optional Texture).
 */
