# Getting started

Setup, build, and project structure. For architecture and plans see [README.md](README.md) and [architecture.md](architecture.md).

---

## Platforms

| Platform | Vulkan / MoltenVK | In this repo |
|----------|-------------------|--------------|
| **Linux** | Native Vulkan (NVIDIA, AMD, Intel) | ✅ Supported |
| **Windows** | Native Vulkan (NVIDIA, AMD, Intel) | ✅ Supported |
| **macOS** | MoltenVK (Vulkan → Metal) | ✅ Supported |
| **iOS** | MoltenVK | Docs + scaffold: [platforms/ios.md](platforms/ios.md) |
| **Android** | Native Vulkan | Docs + scaffold: [platforms/android.md](platforms/android.md) |

On **Linux** and **Windows** we use native Vulkan drivers (no MoltenVK). On **macOS** the Vulkan SDK uses **MoltenVK** (Vulkan → Metal); same Vulkan code. You build **for** each platform **on** that platform.

---

## What to install (per platform)

| Platform | Install once | Then |
|----------|--------------|------|
| **Linux** | Vulkan, SDL3, nlohmann-json, CMake, gcc (e.g. `./setup.sh` or `scripts/linux/setup_linux.sh`) | `./setup.sh` then build |
| **Windows** | [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows), [CMake](https://cmake.org/download/), Visual Studio 2022 (or MinGW), **nlohmann-json** (vcpkg) | `scripts\windows\setup_windows.bat` then build |
| **macOS** | MoltenVK, SDL3, nlohmann-json, CMake (e.g. `./setup.sh` or `brew install molten-vk sdl3 nlohmann-json cmake`) | `./setup.sh` on a Mac then build |
| **Android** | Android Studio + NDK; see [platforms/android.md](platforms/android.md) | Use `platforms/android/` scaffold |
| **iOS** | Xcode, MoltenVK for iOS; see [platforms/ios.md](platforms/ios.md) | Use `platforms/ios/` scaffold |

**Project setup** ensures: Vulkan (SDK/drivers + glslc), SDL3 (window + Vulkan surface), nlohmann-json (config), CMake + C++ compiler. SDL3 can be installed via package manager or let CMake fetch on first build.

---

## Quick start

### Linux

```bash
./setup.sh
scripts/linux/build.sh --debug     # or --release
./install/Debug/bin/VulkanApp     # from project root
```

### Windows

```cmd
scripts\windows\setup_windows.bat
scripts\windows\build.bat --debug     # or --release
install\Debug\bin\VulkanApp.exe       # from project root
```

### macOS

```bash
./setup.sh
scripts/linux/build.sh --debug     # or --release
./install/Debug/bin/VulkanApp
```

**Debug vs Release:** Use `--debug` or `--release`. Debug enables logging and Vulkan validation layers; Release is optimized. Output: `build/Debug`, `build/Release`, `install/Debug`, `install/Release`.

---

## Detailed setup

### Automated (recommended)

- **Linux**: `./setup.sh` (runs `scripts/linux/setup_linux.sh`)
- **Windows**: `scripts\windows\setup_windows.bat`
- **Dependencies**: `scripts/linux/check_dependencies.sh` or `scripts\windows\check_dependencies.bat`

### Manual setup (Linux)

**Arch / CachyOS**

```bash
sudo pacman -S --needed vulkan-devel vulkan-headers vulkan-icd-loader vulkan-validation-layers sdl3 nlohmann-json cmake make gcc base-devel
```

**Ubuntu / Debian**

```bash
sudo apt-get update
sudo apt-get install -y libvulkan-dev vulkan-tools vulkan-validationlayers libsdl3-dev nlohmann-json3-dev cmake build-essential g++
```

**Fedora / RHEL**

```bash
sudo dnf install -y vulkan-devel vulkan-loader vulkan-validation-layers sdl3-devel nlohmann-json-devel cmake gcc-c++ make
```

**openSUSE**

```bash
sudo zypper install -y vulkan-devel vulkan-loader vulkan-validation-layers libSDL3-devel nlohmann-json-devel cmake gcc-c++ make
```

### Manual setup (Windows)

1. **Vulkan SDK** — [vulkan.lunarg.com](https://vulkan.lunarg.com/sdk/home#windows); add `C:\VulkanSDK\<version>\Bin` to PATH.
2. **CMake** — [cmake.org/download](https://cmake.org/download/); add to PATH.
3. **C++** — Visual Studio 2022 (C++ workload), or MinGW-w64, or Clang.
4. **SDL3** — Let CMake fetch, or `vcpkg install sdl3`.
5. **nlohmann-json** — `vcpkg install nlohmann-json`; configure CMake with vcpkg toolchain.

### Manual setup (macOS)

```bash
brew install molten-vk sdl3 nlohmann-json cmake
```

---

## Building

### Automated

- **Linux/macOS**: `scripts/linux/build.sh --debug` or `--release`
- **Windows**: `scripts\windows\build.bat --debug` or `--release`

### Manual (Linux/macOS)

```bash
mkdir -p build/Debug && cd build/Debug
cmake ../.. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
# Run from project root: ./install/Debug/bin/VulkanApp
```

### Manual (Windows)

```cmd
mkdir build\Debug
cd build\Debug
cmake ..\.. -DCMAKE_BUILD_TYPE=Debug -G "Visual Studio 17 2022" -A x64
cmake --build . --config Debug
REM Run from project root: install\Debug\bin\VulkanApp.exe
```

---

## Shader compilation

Shaders are compiled automatically during CMake build if `glslc` or `glslangValidator` is in PATH. Input: `shaders/source/`; output: `build/shaders/`.

**Manual:** `scripts/linux/compile_shaders.sh`, `scripts/macos/compile_shaders.sh`, or `scripts\windows\compile_shaders.bat`. Or: `glslc shaders/source/vert.vert -o build/shaders/vert.spv` (and similarly for frag).

---

## Project structure

```
├── CMakeLists.txt
├── setup.sh
├── README.md
├── docs/                    # Documentation (README.md = index)
│   ├── getting-started.md   # This file
│   ├── troubleshooting.md
│   ├── architecture.md
│   ├── plan-*.md
│   ├── guidelines/
│   ├── vulkan/
│   ├── platforms/
│   └── future-ideas/
├── src/
│   ├── main.cpp
│   ├── app/                 # VulkanApp, main loop
│   ├── config/              # Config loader, VulkanConfig
│   ├── managers/            # PipelineManager
│   ├── scene/               # Object, shapes
│   ├── thread/              # Job queue
│   ├── vulkan/              # Instance, device, swapchain, pipeline, command buffers, sync
│   └── window/              # Window (SDL3)
├── shaders/source/          # GLSL → build/shaders/*.spv
├── scripts/linux/, scripts/windows/, scripts/macos/
└── platforms/android/, platforms/ios/
```

Build output: `build/Debug`, `build/Release`, `install/Debug`, `install/Release`.

---

## Config

Config is loaded from **two files** (paths relative to the executable or working directory): `config/default.json` (read-only defaults, created once) and `config/config.json` (user overrides). See [architecture.md](architecture.md) for the full JSON layout.

**Useful keys:** In `camera`: `use_perspective`, `fov_y_rad`, `near_z`, `far_z`, `ortho_half_extent`, `pan_speed`, `initial_camera_x`, `initial_camera_y`, `initial_camera_z`. In `render`: `cull_back_faces`, `clear_color_r/g/b/a`. Edit `config/config.json` and restart the app to apply (or call `ApplyConfig` at runtime for swapchain-related changes to take effect next frame).

---

## Vulkan vs MoltenVK

| Platform | Vulkan | MoltenVK |
|----------|--------|----------|
| **Linux** | Native (NVIDIA, AMD, Intel) | Not used |
| **Windows** | Native (NVIDIA, AMD, Intel) | Not used |
| **macOS** | No native Vulkan | SDK uses MoltenVK (Vulkan → Metal) |

Same Vulkan code everywhere; MoltenVK only on Apple.

---

## Portable development

1. Clone (or copy) the project.
2. Run setup for your OS: `./setup.sh` or `scripts\windows\setup_windows.bat`.
3. Build: `scripts/linux/build.sh --debug` (or Windows build.bat).
4. Run: `./install/Debug/bin/VulkanApp` or `install\Debug\bin\VulkanApp.exe`.

---

## Shipping / distribution

Resources (shaders, config) are resolved **relative to the executable**.

- **Flat:** One folder with the executable, `shaders/` (`.spv` files), and optionally `config/`. App finds them next to the exe.
- **Install:** `install_local` target produces `install/bin/`, `install/shaders/`, `install/config/`. Run the exe from `install/bin/`.
