#pragma once

/*
 * Managers module: pipeline, material, mesh, texture (and shader manager in vulkan/).
 * See src/managers/README.md and docs/plan-loading-and-managers.md.
 *
 * PipelineManager: get-or-create by key; returns shared_ptr<PipelineHandle>. TrimUnused() + ProcessPendingDestroys() after fence wait. DestroyPipelines() on swapchain recreate.
 * MaterialManager: registry material id -> shared_ptr<MaterialHandle>; materials cache shared_ptr<PipelineHandle>; TrimUnused().
 * MeshManager:     get-or-create procedural by key; returns shared_ptr<MeshHandle> (draw params); TrimUnused().
 * TextureManager:  (future) get-or-load texture by path; VkImage + view + sampler on main thread.
 *
 * Dependency: Shaders (shared_ptr) -> Pipeline -> Material -> Scene. Draw list holds raw VkPipeline/layout.
 */
