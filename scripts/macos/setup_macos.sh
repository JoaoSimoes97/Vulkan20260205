#!/bin/bash

# macOS setup: install Vulkan (MoltenVK) + SDL3 + CMake via Homebrew.
# Run on a Mac to be ready to build. MoltenVK is not installed by the project on Linux/Windows.

set -e

echo "=========================================="
echo "Vulkan App - macOS Setup (MoltenVK)"
echo "=========================================="
echo ""

if ! command -v brew &> /dev/null; then
    echo "Error: Homebrew not found. Install from https://brew.sh"
    exit 1
fi

echo "Project setup: Vulkan (MoltenVK), SDL3 (window/input), nlohmann-json (config), CMake."
echo "Installing Vulkan (MoltenVK), SDL3, nlohmann-json, CMake..."
brew install molten-vk sdl3 nlohmann-json cmake

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
mkdir -p "$ROOT_DIR/deps"
if command -v git &>/dev/null; then
    if [ ! -f "$ROOT_DIR/deps/stb/stb_image.h" ]; then
        echo "Cloning stb into deps/stb..."
        git clone --depth 1 https://github.com/nothings/stb.git "$ROOT_DIR/deps/stb"
    fi
    if [ ! -f "$ROOT_DIR/deps/tinygltf/tiny_gltf.h" ]; then
        echo "Cloning TinyGLTF into deps/tinygltf..."
        git clone --depth 1 --branch v2.9.7 https://github.com/syoyo/tinygltf.git "$ROOT_DIR/deps/tinygltf"
    fi
else
    echo "Warning: git not found; clone stb and TinyGLTF into deps/ manually — see deps/README.md"
fi

echo ""
echo "=========================================="
echo "macOS dependencies installed!"
echo "=========================================="
echo ""
echo "Next: scripts/linux/build.sh --debug  or  --release"
echo "To compile shaders only: scripts/macos/compile_shaders.sh"
echo "Run:  ./install/Debug/bin/VulkanApp"
echo ""
