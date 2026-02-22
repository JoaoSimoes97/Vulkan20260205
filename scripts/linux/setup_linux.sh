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

# Setup vcpkg and bootstrap it (manifest mode will install packages during CMake configure)
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
        # Update vcpkg to latest
        echo "Updating vcpkg..."
        cd "$VCPKG_ROOT" && git pull && cd "$ROOT_DIR"
    fi
    
    # Bootstrap vcpkg if needed
    if [ ! -f "$VCPKG_ROOT/vcpkg" ]; then
        echo "Bootstrapping vcpkg..."
        "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
    fi
    
    echo ""
    echo "vcpkg bootstrapped successfully at $VCPKG_ROOT"
    echo "Dependencies (imgui, imguizmo) will be installed automatically during CMake configure."
    echo ""
    
    # Export VCPKG_ROOT for current session
    export VCPKG_ROOT="$VCPKG_ROOT"
    echo "Set VCPKG_ROOT=$VCPKG_ROOT"
    echo "Add this to your shell profile (~/.bashrc or ~/.zshrc) for persistence:"
    echo "  export VCPKG_ROOT=$VCPKG_ROOT"
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
        base-devel \
        curl \
        zip \
        unzip \
        tar \
        pkg-config
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
        g++ \
        curl \
        zip \
        unzip \
        tar \
        pkg-config
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
        make \
        curl \
        zip \
        unzip \
        tar \
        pkgconf-pkg-config
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
        make \
        curl \
        zip \
        unzip \
        tar \
        pkg-config
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
echo "Setup completed successfully!"
echo "=========================================="
echo ""
echo "Next steps - build the project:"
echo ""
echo "  scripts/linux/build.sh --debug"
echo ""
echo "The build script will automatically:"
echo "  - Use the vcpkg toolchain from $VCPKG_ROOT"
echo "  - Install imgui/imguizmo on first configure"
echo "  - Build and install the application"
echo ""
echo "If you use a different shell session, set VCPKG_ROOT first:"
echo "  export VCPKG_ROOT=$VCPKG_ROOT"
echo ""

