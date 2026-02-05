# Cross-Platform Vulkan Application

A simple cross-platform Vulkan application that renders a red triangle. This project is designed to work on Linux, Windows, and macOS with automated setup scripts.

**Vulkan everywhere, one codebase:** On **Linux** and **Windows** we use **native Vulkan drivers** (NVIDIA, AMD, Intel). On **macOS** (and iOS if you port) the Vulkan SDK uses **MoltenVK** (Vulkan → Metal). Same Vulkan API everywhere.

**Can I use MoltenVK on all PCs?** **No.** MoltenVK is **Apple-only** (it translates Vulkan → Metal). On Linux and Windows there is no Metal, so we use **native Vulkan drivers** — no MoltenVK, zero cost. You cannot "build with Molten on all PCs"; we use Vulkan on Linux/Windows and MoltenVK only when building on macOS/iOS.

### Platforms

| Platform | Vulkan / MoltenVK | In this repo | Build / run |
|----------|-------------------|--------------|-------------|
| **Linux** | Native Vulkan (NVIDIA, AMD, Intel) | ✅ Supported | Setup + build on Linux |
| **Windows** | Native Vulkan (NVIDIA, AMD, Intel) | ✅ Supported | Setup + build on Windows |
| **macOS** | MoltenVK (Vulkan → Metal) | ✅ Supported | Setup + build on a Mac |
| **iOS** | MoltenVK (Vulkan → Metal) | ✅ Docs + scaffold | [docs/platforms/ios.md](docs/platforms/ios.md), `platforms/ios/` |
| **Android** | Native Vulkan | ✅ Docs + scaffold | [docs/platforms/android.md](docs/platforms/android.md), `platforms/android/` |

### Am I ready to build for my platform?

| Platform | Ready when | Build command |
|----------|------------|---------------|
| **Linux** | Run `./setup.sh` on Linux | `scripts/linux/build.sh --debug` or `--release` |
| **Windows** | Run `scripts\windows\setup_windows.bat` and install what it asks | `scripts\windows\build.bat --debug` or `--release` |
| **macOS** | Run `./setup.sh` **on a Mac** (installs MoltenVK + SDL3 + CMake) | `scripts/linux/build.sh --debug` or `--release` |
| **iOS** | Install Xcode + MoltenVK for iOS; see [docs/platforms/ios.md](docs/platforms/ios.md) | Use `platforms/ios/` scaffold + Xcode |
| **Android** | Install Android Studio + NDK; see [docs/platforms/android.md](docs/platforms/android.md) | Use `platforms/android/` scaffold + NDK/CMake |

You build **for** each platform **on** that platform (Linux on Linux, Windows on Windows, macOS on a Mac). After setup for your current OS, you're ready to build for that OS.

### What to install right now (per platform)

| Platform | Install this (once) | Then |
|----------|---------------------|------|
| **Windows** | [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows), [CMake](https://cmake.org/download/), Visual Studio 2022 (or MinGW) with C++ | Run `scripts\windows\setup_windows.bat`, then build |
| **Linux** | Vulkan + SDL3 + CMake + gcc (e.g. `./setup.sh` or `scripts/linux/setup_linux.sh`) | Run `./setup.sh`, then build |
| **macOS** | MoltenVK + SDL3 + CMake (e.g. `./setup.sh` or `brew install molten-vk sdl3 cmake`) | Run `./setup.sh` on a Mac, then build |
| **Android** | [Android Studio](https://developer.android.com/studio) + NDK, or NDK + CMake; Vulkan headers/lib for Android | See [docs/platforms/android.md](docs/platforms/android.md) |
| **iOS** | Xcode (Mac), MoltenVK for iOS, CMake for iOS or Xcode project | See [docs/platforms/ios.md](docs/platforms/ios.md) |

Desktop (Windows, Linux, macOS): install the row above, then use the Quick Start for that OS. Android and iOS: follow the linked docs and use the scaffold in `platforms/android` and `platforms/ios`.

### What project setup installs

Running the project setup (e.g. `./setup.sh` on Linux/macOS or `scripts\windows\setup_windows.bat` on Windows) ensures these are available:

| Component | Purpose |
|-----------|---------|
| **Vulkan** (SDK / drivers + headers) | Graphics API and shader compilation (glslc) |
| **SDL3** | Window, input, and Vulkan surface on all platforms (required; install via package manager or let CMake fetch on first build) |
| **CMake** + C++ compiler | Build system and toolchain |

SDL3 is part of the project setup: on Linux/macOS the setup script installs it; on Windows you can install it via vcpkg or let CMake fetch it when you run the first build.

### Windowing: one API for all?

**Windowing:** We use **SDL3** for window + Vulkan surface on **Linux, Windows, macOS** (and the same API supports **Android** and **iOS** when you build for those platforms).

Vulkan helpers: `SDL_Vulkan_CreateSurface`, `SDL_WINDOW_VULKAN`, `SDL_Vulkan_GetInstanceExtensions`.

## Quick Start

### Linux

1. **Run the setup script** (installs all dependencies automatically):
   ```bash
   ./setup.sh        # auto-detects Linux and runs scripts/linux/setup_linux.sh
   ```

2. **Build the project** (you must pass `--debug` or `--release`):
   ```bash
   scripts/linux/build.sh --debug     # Debug: logging + validation on
   scripts/linux/build.sh --release   # Release: optimized, no logging
   ```

3. **Run the application** (from project root):
   ```bash
   ./install/Debug/bin/VulkanApp      # after Debug build
   ./install/Release/bin/VulkanApp    # after Release build
   ```

**Debug vs Release:** The build script requires an explicit choice: `--debug` or `--release`. **Debug** enables logging and Vulkan validation layers; **Release** disables them and builds with optimizations. Both live under one folder each: `build/Debug`, `build/Release`, `install/Debug`, `install/Release`.

### Windows

1. **Run the setup script** (checks dependencies):
   ```cmd
   scripts\windows\setup_windows.bat
   ```

2. **Build the project** (you must pass `--debug` or `--release`):
   ```cmd
   scripts\windows\build.bat --debug     # Debug: logging + validation on
   scripts\windows\build.bat --release   # Release: optimized, no logging
   ```

3. **Run the application** (from project root):
   ```cmd
   install\Debug\bin\VulkanApp.exe     # after Debug build
   install\Release\bin\VulkanApp.exe    # after Release build
   ```

### macOS (Vulkan via MoltenVK)

On macOS we use the Vulkan SDK, which uses **MoltenVK** (Vulkan → Metal). Same Vulkan code; no change for Linux/Windows.

1. **Run the setup script** (on a Mac; installs MoltenVK + SDL3 + CMake via Homebrew):
   ```bash
   ./setup.sh        # auto-detects macOS and runs scripts/macos/setup_macos.sh
   ```
   Or manually: `brew install molten-vk sdl3 cmake`

2. **Build the project** (you must pass `--debug` or `--release`):
   ```bash
   scripts/linux/build.sh --debug     # Debug: logging + validation on
   scripts/linux/build.sh --release   # Release: optimized, no logging
   ```

3. **Run the application** (from project root):
   ```bash
   ./install/Debug/bin/VulkanApp      # after Debug build
   ./install/Release/bin/VulkanApp    # after Release build
   ```

## Detailed Setup Instructions

### Automated Setup (Recommended)

The project includes automated setup scripts that handle dependency installation:

- **Linux**: `setup_linux.sh` - Automatically detects your distribution and installs all required packages
- **Windows**: `setup_windows.bat` - Checks for dependencies and guides you through installation
- **Dependency Checker**: `check_dependencies.sh` / `check_dependencies.bat` - Verifies all dependencies are installed

### Manual Setup

#### Linux (Arch/CachyOS)

```bash
sudo pacman -S --needed \
    vulkan-devel \
    vulkan-headers \
    vulkan-icd-loader \
    vulkan-validation-layers \
    sdl3 \
    cmake \
    make \
    gcc \
    base-devel
```

#### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
    libvulkan-dev \
    vulkan-tools \
    vulkan-validationlayers \
    libsdl3-dev \
    cmake \
    build-essential \
    g++
```

#### Linux (Fedora/RHEL)

```bash
sudo dnf install -y \
    vulkan-devel \
    vulkan-loader \
    vulkan-validation-layers \
    sdl3-devel \
    cmake \
    gcc-c++ \
    make
```

#### Windows

1. **Install Vulkan SDK**:
   - Download from: https://vulkan.lunarg.com/sdk/home#windows
   - Run the installer
   - Add to PATH: `C:\VulkanSDK\<version>\Bin`

2. **Install CMake**:
   - Download from: https://cmake.org/download/
   - Add to PATH during installation

3. **Install C++ Compiler**:
   - **Option 1**: Visual Studio 2019/2022 (with C++ workload)
   - **Option 2**: MinGW-w64
   - **Option 3**: Clang

4. **SDL3** (required – part of project setup):
   - **Option A**: Let CMake fetch SDL3 on first build (FetchContent).
   - **Option B**: Install via vcpkg: `vcpkg install sdl3`

#### macOS

1. **Install Vulkan SDK**:
   - Download from: https://vulkan.lunarg.com/sdk/home#mac
   - Follow installation instructions

2. **Install dependencies via Homebrew**:
   ```bash
   brew install sdl3 cmake
   ```

## Building the Project

### Automated Build (Recommended)

- **Linux/macOS**: `./build.sh`
- **Windows**: `build.bat`

The build script will:
- Check dependencies
- Create build directory
- Configure CMake
- Compile shaders automatically
- Build the project

### Manual Build

#### Linux/macOS

```bash
# Check dependencies (optional)
./check_dependencies.sh

# Create build directory
mkdir build
cd build

# Configure (CMake will auto-compile shaders if glslc is found)
cmake ..

# Build
make -j$(nproc)

# Run
./VulkanApp
```

#### Windows

```cmd
REM Check dependencies (optional)
check_dependencies.bat

REM Create build directory
mkdir build
cd build

REM Configure (specify your generator)
cmake .. -G "Visual Studio 17 2022"
REM or for MinGW:
REM cmake .. -G "MinGW Makefiles"

REM Build
cmake --build . --config Release

REM Run
.\Release\VulkanApp.exe
```

## Shader Compilation

**Note**: Shaders are automatically compiled during the CMake build process if `glslc` or `glslangValidator` is found in your PATH.

If automatic compilation doesn't work, you can compile shaders manually:

### Linux/macOS

```bash
./compile_shaders.sh
```

### Windows

```cmd
compile_shaders.bat
```

Or manually:
```cmd
glslc shaders\vert.vert -o shaders\vert.spv
glslc shaders\frag.frag -o shaders\frag.spv
```

## Project Structure

```
VulkanProjects/
├── CMakeLists.txt              # CMake build configuration (auto-compiles shaders)
├── setup_linux.sh              # Linux dependency installer
├── setup_windows.bat           # Windows dependency checker
├── setup.sh                    # Master setup script (auto-detects OS)
├── build.sh                    # Automated build script (Linux/macOS)
├── build.bat                   # Automated build script (Windows)
├── check_dependencies.sh       # Dependency checker (Linux/macOS)
├── check_dependencies.bat       # Dependency checker (Windows)
├── compile_shaders.sh          # Shader compiler (Linux/macOS)
├── compile_shaders.bat         # Shader compiler (Windows)
├── README.md                   # This file
├── .gitignore                  # Git ignore file
├── include/
│   ├── vulkan_app.h            # Main application class header
│   └── vulkan_utils.h          # Utility functions header
├── src/
│   ├── main.cpp                # Entry point
│   ├── vulkan_app.cpp          # Main application implementation
│   └── vulkan_utils.cpp        # Utility functions implementation
└── shaders/
    ├── vert.vert               # Vertex shader (GLSL source)
    └── frag.frag               # Fragment shader (GLSL source)
    # .spv files are generated automatically during build
```

## Platforms: Vulkan vs MoltenVK

| Platform | Vulkan | MoltenVK | Cost |
|----------|--------|----------|------|
| **Linux** | Native (NVIDIA, AMD, Intel) | Not used | **Zero** |
| **Windows** | Native (NVIDIA, AMD, Intel) | Not used | **Zero** |
| **macOS** | No native Vulkan | Vulkan SDK uses MoltenVK (Vulkan → Metal) | Same Vulkan code |

So: **Linux and Windows have no MoltenVK cost** — we use the system Vulkan driver. **macOS** uses MoltenVK so the same Vulkan code runs; the project is set up for that from the start.

## Features

- ✅ **Cross-platform support** (Linux, Windows, macOS)
- ✅ **Vulkan everywhere** (native on Linux/Windows; MoltenVK on macOS)
- ✅ **Automated setup scripts** for easy installation
- ✅ **Automatic shader compilation** during build
- ✅ **Dependency checking** before building
- ✅ **Vulkan 1.0 API** with modern C++17
- ✅ **SDL3** for cross-platform windowing (Linux, Windows, macOS; same API for Android/iOS)
- ✅ **Validation layers** (optional, enabled in debug builds)
- ✅ **Window state in real time**: resize, minimize, maximize, restore, display change, fullscreen enter/leave — all handled in the event loop; Vulkan must recreate swapchain and skip rendering when minimized. See [docs/vulkan/swapchain-rebuild-cases.md](docs/vulkan/swapchain-rebuild-cases.md) and `vulkan_app.h` for all rebuild cases (including Vulkan `VK_ERROR_OUT_OF_DATE_KHR` / device lost).
- ✅ **Simple triangle rendering** as a starting point

### Code style

Naming, comments, and class style follow [docs/guidelines/coding-guidelines.md](docs/guidelines/coding-guidelines.md): camelCase, single-line comments (except function/module descriptions), `this->` in classes, optional type prefix for variables (`p`, `n`, `b`). Full doc index: [docs/README.md](docs/README.md).

## Troubleshooting

### "WARNING: radv is not a conformant Vulkan implementation"

On **Linux with an AMD GPU**, the Mesa **RADV** driver may print this to the console when Vulkan initializes. It is **harmless**: RADV is widely used and works well for development. The message is hardcoded in the driver and cannot be disabled from the application. You can safely ignore it. If you need an officially conformant driver for certification, use AMD’s proprietary driver or AMDVLK instead of RADV.

### "Validation layers requested, but not available!"

This is a warning, not an error. The application will continue without validation layers. To install them:

- **Arch/CachyOS**: `sudo pacman -S vulkan-validation-layers`
- **Debian/Ubuntu**: `sudo apt-get install vulkan-validationlayers`
- **Fedora**: `sudo dnf install vulkan-validation-layers`

### CMake can't find Vulkan

**Linux**: Make sure Vulkan development packages are installed:
```bash
./check_dependencies.sh
```

**Windows**: Ensure Vulkan SDK is installed and in PATH:
```cmd
where glslc
```
If not found, add `C:\VulkanSDK\<version>\Bin` to your PATH.

### CMake can't find SDL3

CMake will fetch SDL3 via FetchContent if not found. Alternatively install SDL3:

- **Linux**: `sudo pacman -S sdl3` (Arch), `sudo apt install libsdl3-dev` (Debian/Ubuntu)
- **Windows**: Use vcpkg: `vcpkg install sdl3`
- **macOS**: `brew install sdl3`

### Shaders not compiling automatically

If automatic shader compilation fails:

1. Ensure `glslc` or `glslangValidator` is in your PATH
2. Compile manually using `compile_shaders.sh` or `compile_shaders.bat`
3. The compiled `.spv` files must be in the `build/shaders/` directory

### Build errors

1. **Check dependencies**: Run `check_dependencies.sh` or `check_dependencies.bat`
2. **Clean build**: Delete `build/` directory and rebuild
3. **Check compiler**: Ensure a C++17 compatible compiler is installed

## Portable Development

This project is designed to be portable. To set up on a new machine:

1. **Clone/copy the project** to the new machine
2. **Run the appropriate setup script**:
   - Linux: `./setup_linux.sh`
   - Windows: `setup_windows.bat`
3. **Build**: `./build.sh` or `build.bat`
4. **Run**: `./build/VulkanApp` or `build\Release\VulkanApp.exe`

All dependencies are managed through package managers or the setup scripts, making it easy to set up on any supported platform.

## Next Steps

- Add vertex buffers for more complex geometry
- Implement texture mapping
- Add uniform buffers for transformations
- Implement depth testing
- Add more advanced rendering features
- Add input handling (keyboard/mouse)

## License

This is a simple example project. Feel free to use and modify as needed.
