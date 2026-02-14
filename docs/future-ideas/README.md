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

- **Depth and multi-viewport** – Planned as Phase 1.5 in [plan-editor-and-scene.md](../plan-editor-and-scene.md): render pass descriptor (color + depth), framebuffers with attachment list, Record(render area, viewport, clear array). Enables depth for 3D and ImGui multi-viewport.
- **Vertex buffers** – Render more complex geometry with real vertex data (Phase 2: MeshManager).
- **Textures** – Load and sample textures (Phase 5: descriptors).
- **Uniform buffers** – Push transforms (MVP, etc.) to shaders; descriptor set layouts in pipeline.

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
