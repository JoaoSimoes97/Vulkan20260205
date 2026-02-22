# Third-party dependencies (no download during build)

This directory is **populated by the project setup scripts** so that CMake never downloads anything during build.

## What goes here

| Directory   | Source | Version | Populated by |
|------------|--------|---------|--------------|
| `stb/`     | [nothings/stb](https://github.com/nothings/stb) | latest | Setup scripts |
| `tinygltf/`| [syoyo/tinygltf](https://github.com/syoyo/tinygltf) | v2.9.7 | Setup scripts |
| `imgui/`   | [ocornut/imgui](https://github.com/ocornut/imgui) | v1.91.9-docking | Setup scripts |
| `imguizmo/`| [CedricGuillemet/ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) | ba662b119d64 (2024-05-29) | Setup scripts |

**Version policy:** All versions must match vcpkg (Windows) for cross-platform API compatibility. Check `vcpkg_installed/vcpkg/info/` for current versions.

## If you didn't run setup

Run the setup script for your platform first:

- **Linux:** `./setup.sh` or `scripts/linux/setup_linux.sh`
- **macOS:** `./setup.sh` or `scripts/macos/setup_macos.sh`
- **Windows:** `scripts\windows\setup_windows.bat` (uses vcpkg, no deps/ needed)

Setup installs system packages (Vulkan, SDL3, nlohmann-json, etc.) and clones dependencies into `deps/`. After that, build with CMake only (no network).

## Manual clone (if setup didn't clone or you prefer manual)

```bash
mkdir -p deps
cd deps

# stb (header-only)
git clone --depth 1 https://github.com/nothings/stb.git

# TinyGLTF (glTF loader)
git clone --depth 1 --branch v2.9.7 https://github.com/syoyo/tinygltf.git

# imgui (docking branch, pinned version)
git clone --branch docking https://github.com/ocornut/imgui.git
(cd imgui && git checkout v1.91.9-docking 2>/dev/null || echo "Using latest docking")

# ImGuizmo (pinned commit)
git clone https://github.com/CedricGuillemet/ImGuizmo.git imguizmo
(cd imguizmo && git checkout ba662b119d64f9ab700bb2cd7b2781f9044f5565 2>/dev/null || echo "Using latest")
```

Then configure and build as usual; CMake will use `deps/` and will not fetch anything.
