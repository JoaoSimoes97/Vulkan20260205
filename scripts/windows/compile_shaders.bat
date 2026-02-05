@echo off
REM Script to compile GLSL shaders to SPIR-V on Windows (organized under scripts/windows)
REM Requires glslc from Vulkan SDK

set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%\..
set ROOT_DIR=%ROOT_DIR%\..
cd /d "%ROOT_DIR%"

set SHADER_DIR=shaders
set OUTPUT_DIR=shaders

REM Check for glslc
where glslc >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Error: glslc not found!
    echo Please install the Vulkan SDK and add it to your PATH
    exit /b 1
)

echo Using glslc compiler

REM Compile vertex shader
glslc %SHADER_DIR%\vert.vert -o %OUTPUT_DIR%\vert.spv
if %ERRORLEVEL% NEQ 0 (
    echo Error compiling vertex shader!
    exit /b 1
)

REM Compile fragment shader
glslc %SHADER_DIR%\frag.frag -o %OUTPUT_DIR%\frag.spv
if %ERRORLEVEL% NEQ 0 (
    echo Error compiling fragment shader!
    exit /b 1
)

echo Shaders compiled successfully!

