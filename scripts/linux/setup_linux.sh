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
echo "Then populates deps/ with stb and TinyGLTF, and uses vcpkg for imgui/imguizmo."
echo ""

# vcpkg location
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"

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

# Setup vcpkg and install imgui/imguizmo dependencies
setup_vcpkg() {
    echo ""
    echo "Setting up vcpkg for imgui and imguizmo..."
    
    if ! command -v git &>/dev/null; then
        echo "Error: git is required for vcpkg. Please install git and run setup again."
        return 1
    fi
    
    # Clone vcpkg if not present
    if [ ! -d "$VCPKG_ROOT" ]; then
        echo "Cloning vcpkg to $VCPKG_ROOT..."
        git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
    else
        echo "vcpkg already present at $VCPKG_ROOT"
    fi
    
    # Bootstrap vcpkg if needed
    if [ ! -f "$VCPKG_ROOT/vcpkg" ]; then
        echo "Bootstrapping vcpkg..."
        "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
    fi
    
    # Install dependencies from vcpkg.json using manifest mode
    echo "Installing vcpkg dependencies (imgui, imguizmo)..."
    cd "$ROOT_DIR"
    "$VCPKG_ROOT/vcpkg" install --triplet x64-linux
    
    echo ""
    echo "vcpkg dependencies installed successfully!"
    echo ""
    echo "NOTE: When building, use the vcpkg toolchain file:"
    echo "  cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
    echo ""
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
echo "Populating deps/ (stb, TinyGLTF)..."
populate_deps

echo ""
echo "Setting up vcpkg (imgui, imguizmo)..."
setup_vcpkg

echo ""
echo "=========================================="
echo "Dependencies installed successfully!"
echo "=========================================="
echo ""
echo "Next steps:"
echo "1. Configure project with vcpkg toolchain:"
echo "   cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
echo "2. Build project: cmake --build build"
echo "3. Run: ./install/bin/VulkanApp"
echo ""
echo "Or use the build script: scripts/linux/build.sh"
echo ""

