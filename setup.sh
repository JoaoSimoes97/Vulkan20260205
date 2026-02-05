#!/bin/bash

# Master setup script - detects OS and runs appropriate setup

echo "=========================================="
echo "Vulkan App - Cross-Platform Setup"
echo "=========================================="
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}" && pwd)"

cd "$ROOT_DIR"

# Detect operating system
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "Detected: Linux"
    echo "Running Linux setup script..."
    echo ""
    chmod +x scripts/linux/setup_linux.sh
    scripts/linux/setup_linux.sh
elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Detected: macOS"
    echo "Running macOS setup (MoltenVK + SDL3 + CMake)..."
    echo ""
    chmod +x scripts/macos/setup_macos.sh
    scripts/macos/setup_macos.sh
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
    echo "Detected: Windows"
    echo "Please run: scripts\\windows\\setup_windows.bat"
    echo "Or follow the Windows setup instructions in README.md"
else
    echo "Unknown operating system: $OSTYPE"
    echo "Please follow manual setup instructions in README.md"
    exit 1
fi
