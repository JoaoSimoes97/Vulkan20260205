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

echo ""
echo "=========================================="
echo "macOS dependencies installed!"
echo "=========================================="
echo ""
echo "Next: scripts/linux/build.sh --debug  or  --release"
echo "Run:  ./install/Debug/bin/VulkanApp"
echo ""
