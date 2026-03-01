@echo off
REM Cross-platform build script for Windows (organized under scripts/windows)
REM Usage: scripts\windows\build.bat --debug | scripts\windows\build.bat --release
REM   You must pass either --debug or --release (no default).

set BUILD_TYPE=
set BUILD_DIR=
set INSTALL_DIR=

if "%1"=="--debug" (
    set BUILD_TYPE=Debug
    set BUILD_DIR=build\Debug
    set INSTALL_DIR=install\Debug
)
if "%1"=="--release" (
    set BUILD_TYPE=Release
    set BUILD_DIR=build\Release
    set INSTALL_DIR=install\Release
)

if "%BUILD_TYPE%"=="" (
    echo Usage: scripts\windows\build.bat --debug ^| scripts\windows\build.bat --release
    echo   --debug   Debug build: logging + validation on, debug symbols
    echo   --release Release build: optimized, no logging
    pause
    exit /b 1
)

echo ==========================================
echo Building Vulkan App (%BUILD_TYPE%)
echo ==========================================
echo.

set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%\..
set ROOT_DIR=%ROOT_DIR%\..
cd /d "%ROOT_DIR%"

REM Check dependencies
if exist scripts\windows\check_dependencies.bat (
    call scripts\windows\check_dependencies.bat
    if errorlevel 1 (
        echo.
        echo Some dependencies are missing. Please run:
        echo   scripts\windows\setup_windows.bat
        echo   or follow setup instructions in README.md
        pause
        exit /b 1
    )
    echo.
)

REM Create build directory for this config (build\Debug or build\Release)
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM Configure with CMake (project root is two levels up: build\Debug -> .. -> build, ..\.. -> project root)
echo Configuring with CMake (%BUILD_TYPE%)...
set VCPKG_TOOLCHAIN=%ROOT_DIR%\toolchain\windows\vcpkg\scripts\buildsystems\vcpkg.cmake
if not defined CMAKE_GENERATOR (
    cmake ..\.. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_TOOLCHAIN_FILE=%VCPKG_TOOLCHAIN% -DINSTALL_OUTPUT_DIR=install/%BUILD_TYPE%
) else (
    cmake ..\.. -G "%CMAKE_GENERATOR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_TOOLCHAIN_FILE=%VCPKG_TOOLCHAIN% -DINSTALL_OUTPUT_DIR=install/%BUILD_TYPE%
)

if errorlevel 1 (
    echo.
    echo CMake configuration failed!
    echo Try specifying a generator:
    echo   cmake ..\.. -G "Visual Studio 17 2022" -A x64
    echo   cmake ..\.. -G "MinGW Makefiles"
    pause
    exit /b 1
)

REM Build
echo.
echo Building project...
cmake --build . --config %BUILD_TYPE%

if errorlevel 1 (
    echo.
    echo Build failed!
    pause
    exit /b 1
)

REM Create local install bundle in install\Debug or install\Release
echo.
echo Creating local install bundle in %INSTALL_DIR% ...
cmake --build . --target install_local

echo.
echo ==========================================
echo Build complete (%BUILD_TYPE%)!
echo ==========================================
echo.
echo Run the application with:
echo   From project root:   %INSTALL_DIR%\bin\VulkanApp.exe
echo   From this dir:       .\%BUILD_TYPE%\VulkanApp.exe
echo.
echo To build the other type: scripts\windows\build.bat --release  or  --debug
echo.
