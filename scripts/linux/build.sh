#!/bin/bash

# Cross-platform build script (Linux/macOS), organized under scripts/linux
# Usage: scripts/linux/build.sh --debug | scripts/linux/build.sh --release
#   You must pass either --debug or --release (no default).

set -e

BUILD_TYPE=
BUILD_DIR=
INSTALL_DIR=

for arg in "$@"; do
    case "$arg" in
        --release)
            BUILD_TYPE=Release
            BUILD_DIR="build/Release"
            INSTALL_DIR="install/Release"
            ;;
        --debug)
            BUILD_TYPE=Debug
            BUILD_DIR="build/Debug"
            INSTALL_DIR="install/Debug"
            ;;
    esac
done

if [ -z "$BUILD_TYPE" ]; then
    echo "Usage: scripts/linux/build.sh --debug | scripts/linux/build.sh --release"
    echo "  --debug   Debug build: logging + validation on, debug symbols"
    echo "  --release Release build: optimized, no logging"
    exit 1
fi

echo "=========================================="
echo "Building Vulkan App (${BUILD_TYPE})"
echo "=========================================="
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

cd "$ROOT_DIR"

# Check dependencies first
if [ -f "$ROOT_DIR/scripts/linux/check_dependencies.sh" ]; then
    chmod +x "$ROOT_DIR/scripts/linux/check_dependencies.sh"
    if ! "$ROOT_DIR/scripts/linux/check_dependencies.sh"; then
        echo ""
        echo "Some dependencies are missing. Please run:"
        echo "  scripts/linux/setup_linux.sh  (Linux)"
        echo "  or follow setup instructions in README.md"
        exit 1
    fi
    echo ""
fi

# Create build directory for this config (build/Debug or build/Release)
mkdir -p "$BUILD_DIR"

cd "$BUILD_DIR"

# Configure with CMake (project root is two levels up: build/Debug -> .. -> build, ../.. -> project root)
echo "Configuring with CMake (${BUILD_TYPE})..."
cmake ../.. -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DINSTALL_OUTPUT_DIR="${INSTALL_DIR}"

# Build
echo ""
echo "Building project..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "Creating local install bundle in ./${INSTALL_DIR} ..."
cmake --build . --target install_local

echo ""
echo "=========================================="
echo "Build complete (${BUILD_TYPE})!"
echo "=========================================="
echo ""
echo "Run the application with:"
echo "  ./${INSTALL_DIR}/bin/VulkanApp"
echo "  Or from this dir: ./VulkanApp"
echo ""
echo "To build the other type: scripts/linux/build.sh --release  or  --debug"
echo ""

