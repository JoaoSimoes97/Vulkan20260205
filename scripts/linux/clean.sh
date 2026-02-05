#!/bin/bash

# Clean build and install artifacts for a fresh build/install (organized under scripts/linux)

set -e

echo "=========================================="
echo "Cleaning build and install artifacts"
echo "=========================================="
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

cd "$ROOT_DIR"

rm -rf build install

echo "Removed:"
echo "  - build/ (Debug + Release)"
echo "  - install/ (Debug + Release)"
echo ""
echo "You can now run:"
echo "  scripts/linux/build.sh --debug     # Debug (logging + validation)"
echo "  scripts/linux/build.sh --release   # Release (optimized, no logging)"
echo "for a fresh build and install."

