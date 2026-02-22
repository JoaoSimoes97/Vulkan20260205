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

# ============================================================================
# Dependency versions - MUST match vcpkg versions for cross-platform compatibility
# Check vcpkg_installed/vcpkg/info/ for current Windows versions
# ============================================================================
IMGUI_TAG="v1.91.9-docking"          # vcpkg: imgui_1.91.9 (docking branch)
IMGUIZMO_COMMIT="ba662b119d64f9ab700bb2cd7b2781f9044f5565"  # vcpkg: imguizmo_2024-05-29
TINYGLTF_TAG="v2.9.7"                # vcpkg doesn't use this, but pinned for consistency
# ============================================================================

echo "Project setup: Vulkan (MoltenVK), SDL3 (window/input), nlohmann-json (config), GLM (math library), CMake."
echo "Installing Vulkan (MoltenVK), SDL3, nlohmann-json, GLM, CMake..."
brew install molten-vk sdl3 nlohmann-json glm cmake

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
mkdir -p "$ROOT_DIR/deps"

if command -v git &>/dev/null; then
    # stb (header-only image loading)
    if [ ! -f "$ROOT_DIR/deps/stb/stb_image.h" ]; then
        echo "Cloning stb into deps/stb..."
        git clone --depth 1 https://github.com/nothings/stb.git "$ROOT_DIR/deps/stb"
    else
        echo "deps/stb already present, skipping."
    fi
    
    # TinyGLTF (glTF loader)
    if [ ! -f "$ROOT_DIR/deps/tinygltf/tiny_gltf.h" ]; then
        echo "Cloning TinyGLTF ($TINYGLTF_TAG) into deps/tinygltf..."
        git clone --depth 1 --branch "$TINYGLTF_TAG" https://github.com/syoyo/tinygltf.git "$ROOT_DIR/deps/tinygltf"
    else
        echo "deps/tinygltf already present, skipping."
    fi
    
    # imgui (docking branch, pinned version for vcpkg compatibility)
    if [ ! -f "$ROOT_DIR/deps/imgui/imgui.h" ]; then
        echo "Cloning imgui ($IMGUI_TAG) into deps/imgui..."
        git clone --branch docking https://github.com/ocornut/imgui.git "$ROOT_DIR/deps/imgui"
        (cd "$ROOT_DIR/deps/imgui" && git checkout "$IMGUI_TAG" 2>/dev/null || echo "Tag $IMGUI_TAG not found, using latest docking branch")
    else
        echo "deps/imgui already present, skipping."
    fi
    
    # ImGuizmo (transform gizmos, pinned commit for vcpkg compatibility)
    if [ ! -f "$ROOT_DIR/deps/imguizmo/ImGuizmo.h" ]; then
        echo "Cloning ImGuizmo (commit $IMGUIZMO_COMMIT) into deps/imguizmo..."
        git clone https://github.com/CedricGuillemet/ImGuizmo.git "$ROOT_DIR/deps/imguizmo"
        (cd "$ROOT_DIR/deps/imguizmo" && git checkout "$IMGUIZMO_COMMIT" 2>/dev/null || echo "Commit $IMGUIZMO_COMMIT not found, using latest")
    else
        echo "deps/imguizmo already present, skipping."
    fi
else
    echo "Warning: git not found; clone stb, TinyGLTF, imgui, and ImGuizmo into deps/ manually — see deps/README.md"
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
