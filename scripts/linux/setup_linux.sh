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
echo "Then populates deps/ with stb and TinyGLTF (no download during build)."
echo ""

# Populate deps/ so CMake never downloads during build
populate_deps() {
    local deps_dir="$ROOT_DIR/deps"
    mkdir -p "$deps_dir"
    if ! command -v git &>/dev/null; then
        echo "Warning: git not found; skipping deps/ (stb, TinyGLTF). Install git and re-run setup, or clone deps manually — see deps/README.md"
        return 0
    fi
    if [ ! -f "$deps_dir/stb/stb_image.h" ]; then
        echo "Cloning stb into deps/stb..."
        git clone --depth 1 https://github.com/nothings/stb.git "$deps_dir/stb"
    else
        echo "deps/stb already present, skipping."
    fi
    if [ ! -f "$deps_dir/tinygltf/tiny_gltf.h" ]; then
        echo "Cloning TinyGLTF into deps/tinygltf..."
        git clone --depth 1 --branch v2.9.7 https://github.com/syoyo/tinygltf.git "$deps_dir/tinygltf"
    else
        echo "deps/tinygltf already present, skipping."
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
            echo "  - CMake"
            echo "  - C++ compiler (gcc/g++)"
            exit 1
        fi
        ;;
esac

echo ""
echo "Populating deps/ (stb, TinyGLTF)..."
populate_deps

echo ""
echo "=========================================="
echo "Dependencies installed successfully!"
echo "=========================================="
echo ""
echo "Next steps:"
echo "1. Build project: scripts/linux/build.sh"
echo "2. Run: ./install/bin/VulkanApp"
echo ""

