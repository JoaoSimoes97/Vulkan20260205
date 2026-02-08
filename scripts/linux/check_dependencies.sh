#!/bin/bash

# Dependency checker script for Linux/macOS (organized under scripts/linux)

echo "=========================================="
echo "Checking Dependencies"
echo "=========================================="
echo ""

ERRORS=0
WARNINGS=0

# Check for CMake
if command -v cmake &> /dev/null; then
    CMAKE_VERSION=$(cmake --version | head -n1)
    echo "[OK] CMake: $CMAKE_VERSION"
else
    echo "[ERROR] CMake not found!"
    echo "        Install: sudo pacman -S cmake (Arch)"
    echo "                 sudo apt-get install cmake (Debian/Ubuntu)"
    ERRORS=$((ERRORS + 1))
fi

# Check for C++ compiler
if command -v g++ &> /dev/null; then
    GCC_VERSION=$(g++ --version | head -n1)
    echo "[OK] C++ Compiler: $GCC_VERSION"
elif command -v clang++ &> /dev/null; then
    CLANG_VERSION=$(clang++ --version | head -n1)
    echo "[OK] C++ Compiler: $CLANG_VERSION"
else
    echo "[ERROR] C++ compiler not found!"
    echo "        Install: sudo pacman -S gcc (Arch)"
    echo "                 sudo apt-get install build-essential (Debian/Ubuntu)"
    ERRORS=$((ERRORS + 1))
fi

# Check for Vulkan SDK
if command -v glslc &> /dev/null; then
    echo "[OK] Vulkan SDK (glslc found)"
elif command -v glslangValidator &> /dev/null; then
    echo "[OK] Vulkan SDK (glslangValidator found)"
else
    echo "[WARNING] Vulkan SDK shader compiler not found!"
    echo "          Install: sudo pacman -S vulkan-devel (Arch)"
    echo "                   sudo apt-get install vulkan-tools (Debian/Ubuntu)"
    WARNINGS=$((WARNINGS + 1))
fi

# Check for Vulkan headers
if [ -f /usr/include/vulkan/vulkan.h ] || [ -f /usr/local/include/vulkan/vulkan.h ]; then
    echo "[OK] Vulkan headers found"
else
    echo "[ERROR] Vulkan headers not found!"
    echo "        Install: sudo pacman -S vulkan-headers (Arch)"
    echo "                 sudo apt-get install libvulkan-dev (Debian/Ubuntu)"
    ERRORS=$((ERRORS + 1))
fi

# Check for SDL3
if pkg-config --exists sdl3 2>/dev/null; then
    SDL3_VERSION=$(pkg-config --modversion sdl3)
    echo "[OK] SDL3: $SDL3_VERSION"
elif [ -f /usr/include/SDL3/SDL.h ] || [ -f /usr/local/include/SDL3/SDL.h ]; then
    echo "[OK] SDL3 headers found"
else
    echo "[WARNING] SDL3 not found via pkg-config"
    echo "          CMake will fetch SDL3 via FetchContent, or install:"
    echo "          sudo pacman -S sdl3 (Arch)"
    echo "          sudo apt-get install libsdl3-dev (Debian/Ubuntu)"
    WARNINGS=$((WARNINGS + 1))
fi

# Check for nlohmann/json (required for config)
if pkg-config --exists nlohmann_json 2>/dev/null; then
    JSON_VERSION=$(pkg-config --modversion nlohmann_json 2>/dev/null || true)
    echo "[OK] nlohmann-json: ${JSON_VERSION:-found}"
elif [ -f /usr/include/nlohmann/json.hpp ] || [ -f /usr/local/include/nlohmann/json.hpp ]; then
    echo "[OK] nlohmann-json headers found"
else
    echo "[ERROR] nlohmann-json not found!"
    echo "        Install: sudo pacman -S nlohmann-json (Arch)"
    echo "                 sudo apt-get install nlohmann-json3-dev (Debian/Ubuntu)"
    echo "                 sudo dnf install nlohmann-json-devel (Fedora)"
    echo "                 sudo zypper install nlohmann-json-devel (openSUSE)"
    ERRORS=$((ERRORS + 1))
fi

# Check for validation layers (optional)
if [ -d /usr/share/vulkan/explicit_layer.d ] || [ -d /usr/local/share/vulkan/explicit_layer.d ]; then
    if ls /usr/share/vulkan/explicit_layer.d/*validation* 2>/dev/null | grep -q . || \
       ls /usr/local/share/vulkan/explicit_layer.d/*validation* 2>/dev/null | grep -q .; then
        echo "[OK] Vulkan validation layers found"
    else
        echo "[INFO] Vulkan validation layers not found (optional, but recommended)"
        echo "       Install: sudo pacman -S vulkan-validation-layers (Arch)"
        echo "                sudo apt-get install vulkan-validationlayers (Debian/Ubuntu)"
    fi
else
    echo "[INFO] Vulkan validation layers directory not found (optional)"
fi

echo ""
echo "=========================================="
if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo "All dependencies are installed!"
    exit 0
elif [ $ERRORS -eq 0 ]; then
    echo "Dependencies OK (some warnings above)"
    exit 0
else
    echo "Found $ERRORS error(s) and $WARNINGS warning(s)"
    echo "Please install missing dependencies and run this script again"
    exit 1
fi

