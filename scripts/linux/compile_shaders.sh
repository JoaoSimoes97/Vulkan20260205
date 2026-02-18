#!/bin/bash

# Script to compile GLSL shaders to SPIR-V (organized under scripts/linux)
# Requires glslc (from Vulkan SDK) or glslangValidator

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

SHADER_SOURCE_DIR="${ROOT_DIR}/shaders/source"
OUTPUT_DIR="${ROOT_DIR}/build/shaders"
mkdir -p "${OUTPUT_DIR}"

# Check for glslc (preferred, from Vulkan SDK)
if command -v glslc &> /dev/null; then
    COMPILER="glslc"
    echo "Using glslc compiler"
# Check for glslangValidator (fallback)
elif command -v glslangValidator &> /dev/null; then
    COMPILER="glslangValidator"
    echo "Using glslangValidator compiler"
else
    echo "Error: Neither glslc nor glslangValidator found!"
    echo "Please install the Vulkan SDK"
    exit 1
fi

# Compile vertex shader
if [ "$COMPILER" = "glslc" ]; then
    glslc "$SHADER_SOURCE_DIR/vert.vert" -o "$OUTPUT_DIR/vert.spv"
    glslc "$SHADER_SOURCE_DIR/frag.frag" -o "$OUTPUT_DIR/frag.spv"
    glslc "$SHADER_SOURCE_DIR/frag_alt.frag" -o "$OUTPUT_DIR/frag_alt.spv"
    glslc "$SHADER_SOURCE_DIR/frag_untextured.frag" -o "$OUTPUT_DIR/frag_untextured.spv"
else
    glslangValidator -V "$SHADER_SOURCE_DIR/vert.vert" -o "$OUTPUT_DIR/vert.spv"
    glslangValidator -V "$SHADER_SOURCE_DIR/frag.frag" -o "$OUTPUT_DIR/frag.spv"
    glslangValidator -V "$SHADER_SOURCE_DIR/frag_alt.frag" -o "$OUTPUT_DIR/frag_alt.spv"
    glslangValidator -V "$SHADER_SOURCE_DIR/frag_untextured.frag" -o "$OUTPUT_DIR/frag_untextured.spv"
fi

echo "Shaders compiled successfully!"

