#!/bin/bash

# Cross-platform Vulkan App - Linux Setup Script (organized under scripts/linux)

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

cd "$ROOT_DIR"

echo "=========================================="
echo "Vulkan App - Linux Setup"
echo "=========================================="
echo ""
echo "This installs: Vulkan (headers, loader, validation), SDL3 (window/input), nlohmann-json (config), CMake, build tools."
echo "Then populates deps/ with stb, TinyGLTF, imgui, and imguizmo."
echo ""

# ============================================================================
# Dependency versions - MUST match vcpkg versions for cross-platform compatibility
# Check vcpkg_installed/vcpkg/info/ for current Windows versions
# ============================================================================
IMGUI_TAG="v1.91.9-docking"          # vcpkg: imgui_1.91.9 (docking branch)
IMGUIZMO_COMMIT="ba662b119d64f9ab700bb2cd7b2781f9044f5565"  # vcpkg: imguizmo_2024-05-29
TINYGLTF_TAG="v2.9.7"                # vcpkg doesn't use this, but pinned for consistency
# ============================================================================

# Populate deps/ so CMake never downloads during build
populate_deps() {
    local deps_dir="$ROOT_DIR/deps"
    mkdir -p "$deps_dir"
    if ! command -v git &>/dev/null; then
        echo "Warning: git not found; skipping deps/. Install git and re-run setup, or clone deps manually — see deps/README.md"
        return 0
    fi
    
    # stb (header-only image loading)
    if [ ! -f "$deps_dir/stb/stb_image.h" ]; then
        echo "Cloning stb into deps/stb..."
        git clone --depth 1 https://github.com/nothings/stb.git "$deps_dir/stb"
    else
        echo "deps/stb already present, skipping."
    fi
    
    # TinyGLTF (glTF loader)
    if [ ! -f "$deps_dir/tinygltf/tiny_gltf.h" ]; then
        echo "Cloning TinyGLTF ($TINYGLTF_TAG) into deps/tinygltf..."
        git clone --depth 1 --branch "$TINYGLTF_TAG" https://github.com/syoyo/tinygltf.git "$deps_dir/tinygltf"
    else
        echo "deps/tinygltf already present, skipping."
    fi
    
    # imgui (docking branch, pinned version for vcpkg compatibility)
    if [ ! -f "$deps_dir/imgui/imgui.h" ]; then
        echo "Cloning imgui ($IMGUI_TAG) into deps/imgui..."
        git clone --branch docking https://github.com/ocornut/imgui.git "$deps_dir/imgui"
        (cd "$deps_dir/imgui" && git checkout "$IMGUI_TAG" 2>/dev/null || echo "Tag $IMGUI_TAG not found, using latest docking branch")
    else
        echo "deps/imgui already present, skipping."
    fi
    
    # ImGuizmo (transform gizmos, pinned commit for vcpkg compatibility)
    if [ ! -f "$deps_dir/imguizmo/ImGuizmo.h" ]; then
        echo "Cloning ImGuizmo (commit $IMGUIZMO_COMMIT) into deps/imguizmo..."
        git clone https://github.com/CedricGuillemet/ImGuizmo.git "$deps_dir/imguizmo"
        (cd "$deps_dir/imguizmo" && git checkout "$IMGUIZMO_COMMIT" 2>/dev/null || echo "Commit $IMGUIZMO_COMMIT not found, using latest")
    else
        echo "deps/imguizmo already present, skipping."
    fi
}

# Detect Linux distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
    DISTRO_LIKE=${ID_LIKE:-}
else
    echo "Error: Cannot detect Linux distribution"
    exit 1
fi

echo "Detected distribution: $DISTRO"
echo ""

# Function to install packages on Arch-based systems
install_arch() {
    echo "Installing dependencies for Arch Linux / CachyOS..."
    sudo pacman -S --needed \
        vulkan-devel \
        vulkan-headers \
        vulkan-icd-loader \
        vulkan-validation-layers \
        sdl3 \
        nlohmann-json \
        glm \
        cmake \
        make \
        gcc \
        base-devel
}

# Function to install packages on Debian/Ubuntu-based systems
install_debian() {
    echo "Installing dependencies for Debian/Ubuntu..."
    sudo apt-get update
    sudo apt-get install -y \
        libvulkan-dev \
        vulkan-tools \
        vulkan-validationlayers \
        libsdl3-dev \
        nlohmann-json3-dev \
        libglm-dev \
        cmake \
        build-essential \
        g++
}

# Function to install packages on Fedora/RHEL-based systems
install_fedora() {
    echo "Installing dependencies for Fedora/RHEL..."
    sudo dnf install -y \
        vulkan-devel \
        vulkan-loader \
        vulkan-validation-layers \
        SDL3-devel \
        nlohmann-json-devel \
        glm-devel \
        cmake \
        gcc-c++ \
        make
}

# Function to install packages on openSUSE
install_opensuse() {
    echo "Installing dependencies for openSUSE..."
    sudo zypper install -y \
        vulkan-devel \
        vulkan-loader \
        vulkan-validation-layers \
        libSDL3-devel \
        nlohmann-json-devel \
        glm-devel \
        cmake \
        gcc-c++ \
        make
}

# Install based on distribution
case "$DISTRO" in
    arch|manjaro|endeavouros|cachyos)
        install_arch
        ;;
    debian|ubuntu|linuxmint|pop)
        install_debian
        ;;
    fedora|rhel|centos)
        install_fedora
        ;;
    opensuse*|sles)
        install_opensuse
        ;;
    *)
        if [[ "$DISTRO_LIKE" == *"debian"* ]] || [[ "$DISTRO_LIKE" == *"ubuntu"* ]]; then
            echo "Detected Debian/Ubuntu-like distribution, using Debian installer..."
            install_debian
        elif [[ "$DISTRO_LIKE" == *"arch"* ]]; then
            echo "Detected Arch-like distribution, using Arch installer..."
            install_arch
        else
            echo "Error: Unsupported distribution: $DISTRO"
            echo "Please install the following packages manually:"
            echo "  - Vulkan SDK/development packages"
            echo "  - SDL3 development packages"
            echo "  - nlohmann-json development packages"
            echo "  - GLM development packages"
            echo "  - CMake"
            echo "  - C++ compiler (gcc/g++)"
            exit 1
        fi
        ;;
esac

echo ""
echo "Populating deps/ (stb, TinyGLTF, imgui, imguizmo)..."
populate_deps

echo ""
echo "=========================================="
echo "Setup completed successfully!"
echo "=========================================="
echo ""
echo "Next steps - build the project:"
echo ""
echo "  scripts/linux/build.sh --debug"
echo ""
echo "Run the application:"
echo "  ./install/Debug/bin/VulkanApp"
echo ""

