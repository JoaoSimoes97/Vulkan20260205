# When to Rebuild Swapchain (and Related Vulkan State)

This document lists all cases that require rebuilding the swapchain and/or dependent Vulkan resources (framebuffers, render pass attachments, etc.). The event loop and Vulkan present/acquire paths must handle these so the app reacts in real time.

## Window / Surface (SDL events → `bFramebufferResized` or `bWindowMinimized`)

| Case | SDL event | Action |
|------|-----------|--------|
| Resize / pixel size changed | `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` | Set `bFramebufferResized`; before next draw call `recreateSwapChain()`. |
| Minimized | `SDL_EVENT_WINDOW_MINIMIZED` | Set `bWindowMinimized`; skip `drawFrame()` until restored (swapchain may report 0 size). |
| Restored / maximized | `SDL_EVENT_WINDOW_RESTORED`, `SDL_EVENT_WINDOW_MAXIMIZED` | Clear `bWindowMinimized`, set `bFramebufferResized`; recreate swapchain. |
| Display / monitor changed | `SDL_EVENT_WINDOW_DISPLAY_CHANGED` | Set `bFramebufferResized`; surface may be on different GPU or monitor; recreate swapchain (and surface if needed). |
| Enter / leave fullscreen | `SDL_EVENT_WINDOW_ENTER_FULLSCREEN`, `SDL_EVENT_WINDOW_LEAVE_FULLSCREEN` | Set `bFramebufferResized`; size and possibly display change; recreate swapchain. |
| DPI / scale change | Usually `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` | Same as resize; set `bFramebufferResized`. |

All of the above are already handled in `vulkan_app.cpp`; Vulkan code must react to `bFramebufferResized` and `bWindowMinimized` in the main loop (see comment in `mainLoop()`).

## Vulkan API return values (in `drawFrame` / present / acquire)

| Return value | Meaning | Action |
|--------------|---------|--------|
| `VK_ERROR_OUT_OF_DATE_KHR` | Swapchain no longer matches surface (e.g. resize, display change). | Call `recreateSwapChain()`, then retry acquire/present. Do **not** treat as fatal. |
| `VK_SUBOPTIMAL_KHR` | Present succeeded but swapchain is no longer optimal (e.g. after resize). | Optionally call `recreateSwapChain()` for optimal behaviour; or ignore and keep presenting. |
| `VK_ERROR_DEVICE_LOST` | GPU/driver lost (e.g. TDR, GPU reset). | May require full re-init (device, swapchain, surface) or exit; handle according to app policy. |

Check return values from `vkAcquireNextImageKHR` and `vkQueuePresentKHR`; on `VK_ERROR_OUT_OF_DATE_KHR` recreate and retry.

## Optional / future cases

- **Present mode change** (e.g. vsync on/off): Recreate swapchain with new `VkPresentModeKHR`.
- **HDR / color space / format change**: Recreate swapchain with new format or color space (e.g. when moving to HDR monitor).
- **Multiple windows**: Each window has its own surface and swapchain; create/destroy swapchain when windows are created/destroyed.

## Summary

1. **Event loop**: Set `bFramebufferResized` or `bWindowMinimized` for all window/display events above.
2. **Before draw**: If `bFramebufferResized`, call `recreateSwapChain()` then clear the flag. If `bWindowMinimized`, skip `drawFrame()`.
3. **In drawFrame**: On `VK_ERROR_OUT_OF_DATE_KHR` from acquire or present, call `recreateSwapChain()` and retry; do not treat as fatal.
