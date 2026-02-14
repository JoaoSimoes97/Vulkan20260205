# Documentation

Overview of project documentation. All paths are relative to the project root.

## Structure

| Path | Contents |
|------|----------|
| [getting-started.md](getting-started.md) | Setup (per platform), build, shader compilation, project structure. |
| [troubleshooting.md](troubleshooting.md) | Common issues: validation layers, Vulkan/SDL3, shaders, build errors. |
| [architecture.md](architecture.md) | Module layout, init/cleanup order, swapchain reconstruction, rendering and draw list, future extensions. |
| [plan-loading-and-managers.md](plan-loading-and-managers.md) | Loader/job system, pipeline/mesh/texture managers, editor and many objects. |
| [plan-rendering-and-materials.md](plan-rendering-and-materials.md) | Draw loop (done), pipeline layout parameterization, blend, scene/draw list, materials roadmap. |
| [plan-editor-and-scene.md](plan-editor-and-scene.md) | Editor and scene: many objects, different GPU data; MeshManager, Scene, RenderListBuilder; phased implementation and optimization. |
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
