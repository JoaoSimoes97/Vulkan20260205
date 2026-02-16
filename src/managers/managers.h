#pragma once

/*
 * Managers module: pipeline, material, mesh, texture, descriptor layout/pool (and shader manager in vulkan/).
 * See src/managers/README.md and docs/plan-loading-and-managers.md.
 *
 * DescriptorSetLayoutManager: register descriptor set layouts by key; used for data-driven pipeline layouts and pool sizing.
 * DescriptorPoolManager: build pool from layout keys, allocate/free sets; main thread only.
 * PipelineManager: get-or-create by key; returns shared_ptr<PipelineHandle>. TrimUnused() + ProcessPendingDestroys() after fence wait. DestroyPipelines() on swapchain recreate.
 * MaterialManager: registry material id -> shared_ptr<MaterialHandle>; materials cache shared_ptr<PipelineHandle>; TrimUnused().
 * MeshManager:     get-or-create procedural by key; returns shared_ptr<MeshHandle> (draw params); TrimUnused().
 * TextureManager:  get-or-load texture by path; VkImage + view + sampler; optional async via JobQueue.
 *
 * Dependency: Shaders (shared_ptr) -> Pipeline -> Material -> Scene. Draw list holds raw VkPipeline/layout. Descriptor sets per pipeline via map (pipelineKey -> sets).
 */
