@echo off
REM Cross-platform Vulkan App - Windows Setup Script (organized under scripts/windows)

echo ==========================================
echo Vulkan App - Windows Setup
echo ==========================================
echo.

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo WARNING: Not running as administrator.
    echo Some steps may require manual intervention.
    echo.
)

echo Checking for required tools...
echo.

REM Check for CMake
where cmake >nul 2>nul
if %errorLevel% neq 0 (
    echo [ERROR] CMake not found!
    echo Please install CMake from: https://cmake.org/download/
    echo Make sure to add it to your PATH during installation.
    pause
    exit /b 1
) else (
    echo [OK] CMake found
    cmake --version
)

echo.

REM Check for C++ compiler
where cl >nul 2>nul
if %errorLevel% equ 0 (
    echo [OK] Visual Studio C++ compiler found
    goto :check_vulkan
)

where g++ >nul 2>nul
if %errorLevel% equ 0 (
    echo [OK] MinGW/GCC compiler found
    goto :check_vulkan
)

echo [WARNING] No C++ compiler detected!
echo Please install one of the following:
echo   - Visual Studio 2019/2022 (with C++ workload)
echo   - MinGW-w64
echo   - Clang
goto :check_vulkan

:check_vulkan
echo.

REM Check for Vulkan SDK
where glslc >nul 2>nul
if %errorLevel% neq 0 (
    echo [WARNING] Vulkan SDK not found or not in PATH!
    echo.
    echo Please install the Vulkan SDK:
    echo   1. Download from: https://vulkan.lunarg.com/sdk/home#windows
    echo   2. Run the installer
    echo   3. Add Vulkan SDK to your PATH:
    echo      - Usually: C:\VulkanSDK\<version>\Bin
    echo   4. Restart this script after installation
    echo.
    set /p continue="Continue anyway? (y/n): "
    if /i not "%continue%"=="y" exit /b 1
) else (
    echo [OK] Vulkan SDK found
    where glslc
)

echo.

REM Check for SDL3 (required; part of project setup)
echo Checking for SDL3 (required for window/input)...
echo [INFO] Install via vcpkg: vcpkg install sdl3
echo.

echo Populating deps\ (stb, TinyGLTF) — no download during build...
set ROOT_DIR=%~dp0..\..
if not exist "%ROOT_DIR%\deps" mkdir "%ROOT_DIR%\deps"
where git >nul 2>nul
if %errorLevel% equ 0 (
    if not exist "%ROOT_DIR%\deps\stb\stb_image.h" (
        echo Cloning stb into deps\stb...
        git clone --depth 1 https://github.com/nothings/stb.git "%ROOT_DIR%\deps\stb"
    )
    if not exist "%ROOT_DIR%\deps\tinygltf\tiny_gltf.h" (
        echo Cloning TinyGLTF into deps\tinygltf...
        git clone --depth 1 --branch v2.9.7 https://github.com/syoyo/tinygltf.git "%ROOT_DIR%\deps\tinygltf"
    )
) else (
    echo [WARNING] git not found. Clone stb and TinyGLTF into deps\ manually — see deps\README.md
)
echo.

echo Checking for nlohmann-json (required for config)...
echo [INFO] nlohmann-json is required. Install via vcpkg: vcpkg install nlohmann-json
echo        Configure CMake with: -DCMAKE_TOOLCHAIN_FILE=^<vcpkg_path^>\scripts\buildsystems\vcpkg.cmake
echo.

echo Checking for GLM (required for graphics math)...
echo [INFO] glm is required. Install via vcpkg: vcpkg install glm
echo.

echo ==========================================
echo Project setup check complete!
echo ==========================================
echo.
echo Required: Vulkan SDK, CMake, C++ compiler, nlohmann-json (vcpkg), SDL3 (vcpkg), glm (vcpkg). Setup populates deps\ with stb and TinyGLTF.
echo.
echo Next steps:
echo 1. Build project (pass --debug or --release):
echo    scripts\windows\build.bat --debug
echo    scripts\windows\build.bat --release
echo 2. Run:
echo    install\Debug\bin\VulkanApp.exe   (after --debug)
echo    install\Release\bin\VulkanApp.exe (after --release)
echo.
pause

