# Architecture and modules

The app is split into modules so it stays maintainable and can grow (multiple cameras, shadow maps, raytracing, etc.) without one giant file.

## Module layout

| Module | Responsibility | Reconstruction |
|--------|----------------|----------------|
| **VulkanConfig** | Runtime options: resolution, present mode, vsync, etc. Changing options sets `bSwapchainDirty` to trigger rebuild. | Config is input to `RecreateSwapchain`. |
| **Window** | SDL init, window, events. Creates Vulkan surface. Exposes `GetFramebufferResized`, `GetWindowMinimized`, `GetDrawableSize`. | Resize/minimize set flags; swapchain module reacts. |
| **VulkanInstance** | Instance, layers, extensions (e.g. from SDL). | Rarely recreated. |
| **VulkanDevice** | Physical device pick, logical device, queues, queue family indices. | Usually fixed; only on device lost / GPU switch. |
| **VulkanSwapchain** | Swapchain, image views, extent, format, present mode. `RecreateSwapchain(config)` tears down and recreates. | Core of reconstruction (resize, present mode, format change). |
| **VulkanRenderPass** | Render pass (attachments, subpasses). Depends on swapchain format. | Recreated when swapchain is recreated (format/extent). |
| **VulkanPipeline** | Graphics pipeline (stub for now). Depends on render pass and extent. | Recreated when swapchain/render pass is recreated. |
| **VulkanFramebuffers** | One framebuffer per swapchain image view. | Recreated when swapchain is recreated. |
| **VulkanApp** | Owns all of the above. Init order, main loop, `RecreateSwapchainAndDependents()`, cleanup. | Orchestrates when to reconstruct. |

## Init and cleanup order

**Init:** Window → Instance → Window.CreateSurface → Device → Swapchain → RenderPass → Pipeline → Framebuffers.

**Cleanup (reverse):** Framebuffers → Pipeline → RenderPass → Swapchain → Device → Window.DestroySurface → Instance → Window.

## When swapchain is recreated

- Window resize / fullscreen / display change → `bFramebufferResized` (or config) → before next draw: `RecreateSwapchainAndDependents()`.
- User changes present mode / vsync / resolution in config → `bSwapchainDirty` → same path.
- In `DrawFrame`, `VK_ERROR_OUT_OF_DATE_KHR` from acquire or present → `RecreateSwapchainAndDependents()` and retry.

## Config and JSON file (all options runtime-applicable)

Config is **always** loaded from a file. Default values live in code only: `GetDefaultConfig()` in `config_loader.cpp`.

- **On startup:** The app calls `LoadConfigFromFileOrCreate("config.json")`. The path is relative to the current working directory.
- **If the file exists:** It is loaded and used.
- **If the file does not exist:** A `config.json` file is created from `GetDefaultConfig()`, a log message is emitted (*"Config file not found at \"config.json\"; created from defaults. Edit the file and restart to change settings."*), and those defaults are used. The user can edit the new file before the next run.

There is no separate example file; the only source of default values is `GetDefaultConfig()`.

**JSON structure** (see `config_loader.h`). Uses **nlohmann/json**.

| Section    | Keys | Values / type |
|------------|------|----------------|
| `window`   | `width`, `height` | unsigned int |
|            | `fullscreen` | bool |
|            | `title` | string |
| `swapchain`| `present_mode` | `fifo` \| `mailbox` \| `immediate` \| `fifo_relaxed` |
|            | `preferred_format` | e.g. `B8G8R8A8_SRGB`, `B8G8R8A8_UNORM`, `R8G8B8A8_SRGB` |
|            | `preferred_color_space` | `SRGB_NONLINEAR`, `DISPLAY_P3`, `EXTENDED_SRGB` |

Validation layers are **not** in the config file (dev/debug only); set from build type or env when you implement them.

- **Resize path**: User resizes window → config is synced from window size → recreate.
- **Config path**: Load JSON or set config, call `ApplyConfig(config)` → window size/fullscreen/title updated, `bSwapchainDirty` set → next frame recreate uses new config.

See [vulkan/swapchain-rebuild-cases.md](vulkan/swapchain-rebuild-cases.md).

## Future extensions (not implemented yet)

- **Multiple cameras** — Separate camera/view modules; each may have its own render target or share swapchain.
- **Shadow maps** — Offscreen render targets (images + framebuffers + pipeline); separate from main swapchain, owned by a dedicated module.
- **Raytracing** — Acceleration structures, raytracing pipeline; separate module, composed in VulkanApp.

New features should be added as new modules (or new submodules under existing ones) rather than overloading a single class.
