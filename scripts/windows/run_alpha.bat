@echo off
REM Build Debug and run with default level (alpha validation).
REM Usage: scripts\windows\run_alpha.bat
REM   Optional: pass a level path, e.g. scripts\windows\run_alpha.bat levels/default/level.json

set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%\..
set ROOT_DIR=%ROOT_DIR%\..
cd /d "%ROOT_DIR%"

set LEVEL_PATH=levels/default/level.json
if not "%~1"=="" set LEVEL_PATH=%~1

call scripts\windows\build.bat --debug
if errorlevel 1 exit /b 1

echo.
echo Launching VulkanApp with %LEVEL_PATH% ...
echo (Run from install\Debug\bin if shaders not found when run from project root.)
"%ROOT_DIR%\install\Debug\bin\VulkanApp.exe" "%LEVEL_PATH%"
