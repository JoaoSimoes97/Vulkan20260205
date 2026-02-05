# Future ideas

Placeholder for later improvements. Not in scope for current learning path.

---

## Logging

- **Async logging** – Use a library (e.g. spdlog) or a lock-free queue + worker thread so log I/O never blocks the render loop.
- **Rate limiting** – Option to log at most every N frames or every N ms (e.g. FPS, state) to avoid flooding.
- **Log to file** – Optional file sink in addition to console (e.g. for crash reports or CI).

---

## Build / DX

- **Package managers** – Optional vcpkg or Conan for dependencies (e.g. SDL3, Vulkan) on Windows.
- **CI** – Optional GitHub Actions (or similar) to build and run on Linux/Windows.

---

## Rendering

- **Vertex buffers** – Render more complex geometry with real vertex data.
- **Textures** – Load and sample textures.
- **Depth buffer** – Proper depth testing for 3D.
- **Uniform buffers** – Push transforms (MVP, etc.) to shaders.

---

## Windowing

- **SDL3** – This project uses SDL3 for window + Vulkan on all platforms (Linux, Windows, macOS; same API for Android/iOS).

---

## UI / ImGui

- **Dear ImGui** – Add after Vulkan is working (triangle on screen, resize, present). Use official backends: SDL3 for input, Vulkan for rendering. Needs: descriptor pool for ImGui, `ImGui_ImplSDL3_InitForVulkan`, `ImGui_ImplVulkan_Init`, then per-frame new frame → UI calls → render → submit ImGui draw data in a render pass. Official example: `imgui/examples/example_sdl3_vulkan`. Good for debug overlays, FPS/stats, tool windows.

---

## Portability

- **macOS** – MoltenVK path and platform notes.
- **Steam Deck / handheld** – Any platform-specific tweaks.

---

## Performance

- **Profiling** – Tracy, RenderDoc, or similar.
- **Hot path** – Reduce allocations and work on the render thread.
