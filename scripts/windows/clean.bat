@echo off
REM Clean build and install artifacts for a fresh build/install (organized under scripts/windows)

echo ==========================================
echo Cleaning build and install artifacts
echo ==========================================
echo.

set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%\..
set ROOT_DIR=%ROOT_DIR%\..
cd /d "%ROOT_DIR%"

if exist build (
    rmdir /s /q build
    echo Removed: build\
)

if exist install (
    rmdir /s /q install
    echo Removed: install\
)

echo.
echo You can now run:
echo   scripts\windows\build.bat --debug     # Debug (logging + validation)
echo   scripts\windows\build.bat --release   # Release (optimized, no logging)
echo for a fresh build and install.
echo.

