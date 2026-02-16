# Cross-Platform Vulkan Application

A cross-platform Vulkan application (SDL3 window, Vulkan 1.4, swapchain, draw list with multiple objects). Linux, Windows, and macOS — one codebase. Native Vulkan on Linux/Windows; MoltenVK (Vulkan → Metal) on macOS.

## Quick start

**Linux / macOS**

```bash
./setup.sh
scripts/linux/build.sh --debug
./install/Debug/bin/VulkanApp levels/default/level.json
```

**Windows**

```cmd
scripts\windows\setup_windows.bat
scripts\windows\build.bat --debug
install\Debug\bin\VulkanApp.exe levels/default/level.json
```

**Note:** Level path is required. See `levels/` folder for available levels.

Use `--release` for an optimized build. Full setup, manual install per platform, and build options: **[docs/getting-started.md](docs/getting-started.md)**.

## Documentation

| Link | Contents |
|------|----------|
| [docs/README.md](docs/README.md) | Documentation index and quick links |
| [docs/getting-started.md](docs/getting-started.md) | Setup, build, project structure |
| [docs/troubleshooting.md](docs/troubleshooting.md) | Common issues and fixes |
| [docs/architecture.md](docs/architecture.md) | Module layout, init order, swapchain |

Android and iOS: [docs/platforms/android.md](docs/platforms/android.md), [docs/platforms/ios.md](docs/platforms/ios.md).

## Features

- Cross-platform (Linux, Windows, macOS), Vulkan 1.4, C++23
- SDL3 window and Vulkan surface; automated setup and build scripts
- Swapchain, render pass, pipeline manager, draw list (multiple objects, per-object color/texture)
- glTF 2.0 support with textures, materials, and PBR properties
- Procedural mesh generation (cube, sphere, cylinder, cone, triangle, rectangle)
- Dynamic descriptor pool management with automatic growth
- Command-line level loading with JSON scene format
- Code style: [docs/guidelines/coding-guidelines.md](docs/guidelines/coding-guidelines.md)

## License

Free to use and modify.
