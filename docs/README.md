# Documentation

Overview of project documentation. All paths are relative to the project root.

## Next up

**Managers and draw path done** — MeshManager, TextureManager (stb_image, async load, TrimUnused), Scene, SceneManager, RenderListBuilder (sort by pipeline/mesh, frustum culling, push size validation), typed JobQueue (LoadFile/LoadMesh/LoadTexture), blend in pipeline params. **Descriptor set infrastructure** in place (layout, pool, one set; pipeline layout supports optional descriptor set layouts). **Next**: materials + textures (bind texture to set, add layout to pipeline, bind set in Record); then instancing. See [plan-editor-and-scene.md](plan-editor-and-scene.md) and [plan-loading-and-managers.md](plan-loading-and-managers.md).

---

## Structure

| Path | Contents |
|------|----------|
| [getting-started.md](getting-started.md) | Setup (per platform), build, shader compilation, project structure. |
| [troubleshooting.md](troubleshooting.md) | Common issues: validation layers, Vulkan/SDL3, shaders, build errors. |
| [architecture.md](architecture.md) | Module layout, init/cleanup order, swapchain reconstruction, config (incl. camera/render), rendering and draw list, implemented (Phase 1.5, camera), future extensions. |
| [plan-loading-and-managers.md](plan-loading-and-managers.md) | Loader/job system (typed jobs, dispatch by type), pipeline/mesh/texture managers (TextureManager done; next: descriptor sets, materials+textures). |
| [plan-rendering-and-materials.md](plan-rendering-and-materials.md) | Draw loop, depth and blend (done), pipeline layout, scene/draw list, frustum culling, push validation; materials/descriptor roadmap. |
| [plan-editor-and-scene.md](plan-editor-and-scene.md) | Editor and scene: Scene, SceneManager, RenderListBuilder (frustum culling, push validation done); Phase 4 instancing, Phase 5 descriptors/textures. |
| [guidelines/](guidelines/) | Code style and conventions (naming, comments, classes, formatting). |
| [vulkan/](vulkan/) | Vulkan implementation: tutorial order, swapchain rebuild cases. |
| [platforms/](platforms/) | Platform-specific setup: Android, iOS. |
| [future-ideas/](future-ideas/) | Possible improvements (logging, build, rendering). |
| [future-ideas/animation-skinning-roadmap.md](future-ideas/animation-skinning-roadmap.md) | Future task plan for real animation/skinning support. |

## Quick links

- **Getting started** — [getting-started.md](getting-started.md)
- **Troubleshooting** — [troubleshooting.md](troubleshooting.md)
- **Architecture / modules** — [architecture.md](architecture.md)
- **Plan: loading and managers** — [plan-loading-and-managers.md](plan-loading-and-managers.md)
- **Plan: rendering and materials** — [plan-rendering-and-materials.md](plan-rendering-and-materials.md)
- **Plan: editor and scene** — [plan-editor-and-scene.md](plan-editor-and-scene.md)
- **Code style** — [guidelines/coding-guidelines.md](guidelines/coding-guidelines.md)
- **Tutorial order** — [vulkan/tutorial-order.md](vulkan/tutorial-order.md)
- **Swapchain rebuild** — [vulkan/swapchain-rebuild-cases.md](vulkan/swapchain-rebuild-cases.md)
- **Vulkan version support (GPUs, Android)** — [vulkan/version-support.md](vulkan/version-support.md)
- **Animation and skinning roadmap (future task)** — [future-ideas/animation-skinning-roadmap.md](future-ideas/animation-skinning-roadmap.md)
- **Android** — [platforms/android.md](platforms/android.md)
- **iOS** — [platforms/ios.md](platforms/ios.md)
