# Troubleshooting

Common issues and fixes. For setup and build see [getting-started.md](getting-started.md).

---

## "SDL_Init failed: no display or video subsystem"

The app needs a **graphical display** to create a window. This error usually means it was started without access to one.

**Do this:**

1. **Run from a terminal inside your desktop session** (logged in to your graphical session), not from SSH without X11/Wayland forwarding, and not from a headless server.
2. **X11:** Ensure `DISPLAY` is set (e.g. `echo $DISPLAY` shows `:0` or similar). If you use SSH, connect with `ssh -X` or `ssh -Y` for X11 forwarding.
3. **Wayland:** Normally the session sets the display; run the app from a terminal in that same session.
4. **WSL:** Use WSLg so the app can open a window, or set `DISPLAY` if you run an X server on Windows.
5. **IDE / launcher:** If your IDE runs the app in a sandbox or without the display env, run `./install/Debug/bin/VulkanApp` (or your build path) from a normal terminal instead.

---

## "WARNING: radv is not a conformant Vulkan implementation"

On **Linux with an AMD GPU**, the Mesa **RADV** driver may print this when Vulkan initializes. It is **harmless**: RADV is widely used and works for development. The message is hardcoded in the driver. You can ignore it. For official conformance use AMD’s proprietary driver or AMDVLK.

---

## "Validation layers requested, but not available!"

Warning only; the app continues without validation. To install:

- **Arch/CachyOS**: `sudo pacman -S vulkan-validation-layers`
- **Debian/Ubuntu**: `sudo apt-get install vulkan-validationlayers`
- **Fedora**: `sudo dnf install vulkan-validation-layers`

---

## CMake can't find Vulkan

**Linux:** Install Vulkan development packages and run `scripts/linux/check_dependencies.sh` (or `./check_dependencies.sh` if at project root).

**Windows:** Install [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows) and add `C:\VulkanSDK\<version>\Bin` to PATH. Check with `where glslc`.

---

## CMake can't find SDL3

CMake can fetch SDL3 via FetchContent. Or install manually:

- **Linux**: `sudo pacman -S sdl3` (Arch), `sudo apt install libsdl3-dev` (Debian/Ubuntu)
- **Windows**: `vcpkg install sdl3`
- **macOS**: `brew install sdl3`

---

## Shaders not compiling automatically

1. Ensure `glslc` or `glslangValidator` is in your PATH.
2. Compile manually: `scripts/linux/compile_shaders.sh`, `scripts/macos/compile_shaders.sh`, or `scripts\windows\compile_shaders.bat`.
3. Put `.spv` files in `build/shaders/` (or the same path your build uses).

---

## Aspect ratio wrong when resizing

The app syncs the swapchain to the current window drawable size **every frame** (not only on resize events), so the rendered image should keep the correct aspect. If the picture still looks stretched or inverted when you resize:

- Ensure you're not overriding or caching an old extent elsewhere.
- Projection uses **aspect = width/height** (drawable size); viewport uses swapchain extent, which is updated from drawable size on recreate. See [architecture.md](architecture.md) (Swapchain extent and aspect ratio).

---

## Build errors

1. **Check dependencies:** `scripts/linux/check_dependencies.sh` or `scripts\windows\check_dependencies.bat`
2. **Clean build:** Delete `build/` and rebuild.
3. **Compiler:** Use a C++23-capable compiler.
