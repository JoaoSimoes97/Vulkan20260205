@echo off
REM Dependency checker script for Windows (organized under scripts/windows)

echo ==========================================
echo Checking Dependencies
echo ==========================================
echo.

set ERRORS=0
set WARNINGS=0

REM Check for CMake
where cmake >nul 2>nul
if %errorLevel% equ 0 (
    echo [OK] CMake found
    cmake --version | findstr /C:"version"
) else (
    echo [ERROR] CMake not found!
    echo         Download from: https://cmake.org/download/
    set /a ERRORS+=1
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

where clang++ >nul 2>nul
if %errorLevel% equ 0 (
    echo [OK] Clang compiler found
    goto :check_vulkan
)

echo [ERROR] C++ compiler not found!
echo         Install Visual Studio 2019/2022 with C++ workload
echo         OR install MinGW-w64
set /a ERRORS+=1

:check_vulkan
echo.

REM Check for Vulkan SDK
where glslc >nul 2>nul
if %errorLevel% equ 0 (
    echo [OK] Vulkan SDK found (glslc)
    where glslc
) else (
    echo [ERROR] Vulkan SDK not found or not in PATH!
    echo         Download from: https://vulkan.lunarg.com/sdk/home#windows
    echo         Add to PATH: C:\VulkanSDK\<version>\Bin
    set /a ERRORS+=1
)

echo.

REM Check for SDL3 (CMake will fetch if not found)
echo [INFO] SDL3 will be fetched by CMake if not found
echo        Or install via vcpkg: vcpkg install sdl3

echo.
echo ==========================================
if %ERRORS% equ 0 (
    echo All dependencies are installed!
    exit /b 0
) else (
    echo Found %ERRORS% error(s)
    echo Please install missing dependencies
    exit /b 1
)

