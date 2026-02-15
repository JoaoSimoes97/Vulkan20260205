# Documentation

Overview of project documentation. All paths are relative to the project root.

## Next up

**Phase 2 done** — MeshManager (procedural + async .obj load/parse), Scene, SceneManager, RenderListBuilder (sort by pipeline/mesh, reuse vector). **Next**: mesh vertex buffer upload and pipeline vertex input; texture manager; instancing. See [plan-editor-and-scene.md](plan-editor-and-scene.md) and [plan-loading-and-managers.md](plan-loading-and-managers.md).

---

## Structure

| Path | Contents |
|------|----------|
| [getting-started.md](getting-started.md) | Setup (per platform), build, shader compilation, project structure. |
| [troubleshooting.md](troubleshooting.md) | Common issues: validation layers, Vulkan/SDL3, shaders, build errors. |
| [architecture.md](architecture.md) | Module layout, init/cleanup order, swapchain reconstruction, config (incl. camera/render), rendering and draw list, implemented (Phase 1.5, camera), future extensions. |
| [plan-loading-and-managers.md](plan-loading-and-managers.md) | Loader/job system, pipeline/mesh/texture managers (Phase 2 done: mesh file load, RenderListBuilder; next: vertex buffer upload, texture manager). |
| [plan-rendering-and-materials.md](plan-rendering-and-materials.md) | Draw loop, depth and multi-viewport prep (done), pipeline layout, blend, scene/draw list, materials roadmap. |
| [plan-editor-and-scene.md](plan-editor-and-scene.md) | Editor and scene: Phase 2 done (Scene, SceneManager, RenderListBuilder, mesh file load); Phase 3/4 (instancing, textures). |
| [guidelines/](guidelines/) | Code style and conventions (naming, comments, classes, formatting). |
| [vulkan/](vulkan/) | Vulkan implementation: tutorial order, swapchain rebuild cases. |
| [platforms/](platforms/) | Platform-specific setup: Android, iOS. |
| [future-ideas/](future-ideas/) | Possible improvements (logging, build, rendering). |

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
- **Android** — [platforms/android.md](platforms/android.md)
- **iOS** — [platforms/ios.md](platforms/ios.md)
