# Quick Setup Guide

This is a quick reference for setting up the project on a new machine.

## One-Command Setup

### Linux
```bash
./setup_linux.sh && ./build.sh
```

### Windows
```cmd
setup_windows.bat
build.bat
```

## Step-by-Step

### 1. Check/Install Dependencies

**Linux:**
```bash
./check_dependencies.sh
# If missing dependencies:
./setup_linux.sh
```

**Windows:**
```cmd
check_dependencies.bat
# If missing dependencies:
setup_windows.bat
```

### 2. Build

**Linux/macOS:**
```bash
./build.sh
```

**Windows:**
```cmd
build.bat
```

### 3. Run

**Linux/macOS:**
```bash
./build/VulkanApp
```

**Windows:**
```cmd
build\Release\VulkanApp.exe
```

## What Gets Installed

### Linux
- Vulkan SDK (development packages)
- GLFW (windowing library)
- CMake (build system)
- GCC/G++ (C++ compiler)
- Vulkan validation layers (optional, for debugging)

### Windows
- Vulkan SDK (manual installation required)
- CMake (manual installation required)
- Visual Studio or MinGW (C++ compiler)
- GLFW (auto-downloaded by CMake)

## Troubleshooting

If setup fails, check:
1. Internet connection (for package downloads)
2. Administrator/sudo permissions
3. Run `check_dependencies.sh` or `check_dependencies.bat` to see what's missing

For detailed instructions, see [README.md](README.md).
