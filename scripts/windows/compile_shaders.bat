@echo off
REM Script to compile GLSL shaders to SPIR-V on Windows (organized under scripts/windows)
REM Input: shaders\source\, Output: build\shaders\ (same as CMake)
REM Requires glslc (preferred) or glslangValidator from Vulkan SDK

set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%\..
set ROOT_DIR=%ROOT_DIR%\..
cd /d "%ROOT_DIR%"

set SHADER_SOURCE_DIR=shaders\source
set OUTPUT_DIR=build\shaders
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM Check for glslc (preferred)
where glslc >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    set COMPILER=glslc
    echo Using glslc compiler
    goto :compile
)

REM Check for glslangValidator (fallback)
where glslangValidator >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    set COMPILER=glslangValidator
    echo Using glslangValidator compiler
    goto :compile
)

echo Error: Neither glslc nor glslangValidator found!
echo Please install the Vulkan SDK and add its Bin directory to your PATH
exit /b 1

:compile
if "%COMPILER%"=="glslc" (
    glslc %SHADER_SOURCE_DIR%\vert.vert -o %OUTPUT_DIR%\vert.spv
    if %ERRORLEVEL% NEQ 0 ( echo Error compiling vertex shader! & exit /b 1 )
    glslc %SHADER_SOURCE_DIR%\frag.frag -o %OUTPUT_DIR%\frag.spv
    if %ERRORLEVEL% NEQ 0 ( echo Error compiling fragment shader! & exit /b 1 )
) else (
    glslangValidator -V %SHADER_SOURCE_DIR%\vert.vert -o %OUTPUT_DIR%\vert.spv
    if %ERRORLEVEL% NEQ 0 ( echo Error compiling vertex shader! & exit /b 1 )
    glslangValidator -V %SHADER_SOURCE_DIR%\frag.frag -o %OUTPUT_DIR%\frag.spv
    if %ERRORLEVEL% NEQ 0 ( echo Error compiling fragment shader! & exit /b 1 )
)

echo Shaders compiled successfully!

