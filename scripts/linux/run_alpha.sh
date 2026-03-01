#!/bin/bash
# Build Debug and run with default level (alpha validation).
# Usage: scripts/linux/run_alpha.sh [level_path]
#   Default level: levels/default/level.json

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
LEVEL_PATH="${1:-levels/default/level.json}"

cd "$ROOT_DIR"
"$SCRIPT_DIR/build.sh" --debug
echo ""
echo "Launching VulkanApp with ${LEVEL_PATH} ..."
"$ROOT_DIR/install/Debug/bin/VulkanApp" "$LEVEL_PATH"
