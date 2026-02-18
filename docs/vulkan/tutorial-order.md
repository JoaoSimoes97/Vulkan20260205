# Vulkan tutorial order — where to add what

Use this as a checklist while following your tutorial. The project already has: window (SDL3), event loop, `bFramebufferResized` / `bWindowMinimized`, and hints in `vulkan_app.cpp`. See [../guidelines/coding-guidelines.md](../guidelines/coding-guidelines.md) for naming and style.

**After Vulkan works:** Dear ImGui is planned (see [../ROADMAP.md](../ROADMAP.md) Future section). Add it once you have a working triangle and swapchain.

## Init order (in `initVulkan()`)

1. **Instance** — `createInstance()`; use `getRequiredExtensions()` (SDL: `SDL_Vulkan_GetInstanceExtensions()`), `checkValidationLayerSupport()`.
2. **Debug** — `setupDebugMessenger()` (validation layers).
3. **Surface** — `createSurface()`; use `SDL_Vulkan_CreateSurface(pWindow, instance, ...)`.
4. **Physical device** — `pickPhysicalDevice()`, `isDeviceSuitable()`, queue families.
5. **Logical device** — `createLogicalDevice()`.
6. **Swapchain** — `createSwapChain()` (choose format, extent, present mode), `createImageViews()`.
7. **Render pass** — `createRenderPass()`.
8. **Pipeline** — `createGraphicsPipeline()`, `createShaderModule()` (load `.spv` from `shaders/`).
9. **Framebuffers** — `createFramebuffers()`.
10. **Command pool & buffers** — `createCommandPool()`, `createCommandBuffers()`.
11. **Sync objects** — `createSyncObjects()` (semaphores, fences).

## Main loop (already in place)

- Events set `bFramebufferResized` and `bWindowMinimized`.
- Before `drawFrame()`: if `bFramebufferResized`, call `recreateSwapChain()`, then clear the flag.
- Skip `drawFrame()` when `bWindowMinimized`.

## Draw (in `drawFrame()`)

- `vkAcquireNextImageKHR` → record command buffer → submit → `vkQueuePresentKHR`.
- On `VK_ERROR_OUT_OF_DATE_KHR` (or `VK_SUBOPTIMAL_KHR`): call `recreateSwapChain()` and retry; do not treat as fatal.

## Recreate swapchain

- `cleanupSwapChain()` then recreate: swapchain, image views, render pass (if dependent), pipeline (if dependent), framebuffers, command buffers.
- Use `oldSwapchain` in `VkSwapchainCreateInfoKHR` for smooth resize.

## Cleanup (in `cleanup()`)

Reverse of init: sync objects → command pool → framebuffers → pipeline → render pass → image views → swapchain → device → surface → debug messenger → instance. Then window and SDL.

## Files

- **Shaders**: `shaders/vert.vert`, `shaders/frag.frag` → built to `build/.../shaders/*.spv`.
- **Rebuild cases**: [swapchain-rebuild-cases.md](swapchain-rebuild-cases.md).
