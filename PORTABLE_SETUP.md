# Portable Development Setup

This document explains how to make your project portable and set it up on any machine.

## What Makes This Project Portable

✅ **Automated Setup Scripts** - Install dependencies automatically  
✅ **Cross-Platform Build System** - CMake works on Linux, Windows, macOS  
✅ **Automatic Shader Compilation** - Shaders compile during build  
✅ **Dependency Checking** - Scripts verify all requirements  
✅ **No Hardcoded Paths** - Everything uses relative paths  
✅ **Package Manager Integration** - Uses system package managers  

## Setting Up on a New Machine

### Option 1: Automated (Recommended)

**Linux:**
```bash
# 1. Copy/clone project to new machine
# 2. Run setup
./setup_linux.sh

# 3. Build
./build.sh

# 4. Run
./build/VulkanApp
```

**Windows:**
```cmd
REM 1. Copy/clone project to new machine
REM 2. Run setup (checks dependencies)
setup_windows.bat

REM 3. Build
build.bat

REM 4. Run
build\Release\VulkanApp.exe
```

### Option 2: Manual

Follow the detailed instructions in [README.md](README.md).

## What to Backup/Transfer

When moving to a new machine, you only need:

```
VulkanProjects/
├── CMakeLists.txt
├── setup_*.sh / setup_*.bat
├── build.sh / build.bat
├── check_dependencies.*
├── compile_shaders.*
├── include/
├── src/
└── shaders/
    ├── vert.vert
    └── frag.frag
```

**Do NOT backup:**
- `build/` directory (will be regenerated)
- `*.spv` files (compiled automatically)
- Any IDE-specific files (if using .gitignore)

## Version Control

If using Git, the `.gitignore` is already configured to exclude:
- Build artifacts
- Compiled shaders
- IDE files
- Temporary files

This keeps your repository clean and portable.

## Platform-Specific Notes

### Linux
- Works on: Arch, Ubuntu, Debian, Fedora, openSUSE, and derivatives
- Setup script auto-detects distribution
- Uses system package manager (pacman, apt, dnf, zypper)

### Windows
- Requires manual Vulkan SDK installation (one-time)
- CMake auto-downloads GLFW if needed
- Works with Visual Studio, MinGW, or Clang

### macOS
- Requires manual Vulkan SDK installation
- Uses Homebrew for dependencies
- Follow macOS setup in README.md

## Reinstalling Everything

If you need to completely reinstall:

1. **Delete build directory**: `rm -rf build/` (Linux) or `rmdir /s build` (Windows)
2. **Run setup script again**: `./setup_linux.sh` or `setup_windows.bat`
3. **Rebuild**: `./build.sh` or `build.bat`

## Troubleshooting New Installations

1. **Run dependency checker first**:
   ```bash
   ./check_dependencies.sh  # Linux
   check_dependencies.bat    # Windows
   ```

2. **Check setup script output** for any errors

3. **Verify PATH** contains:
   - Vulkan SDK binaries (glslc)
   - CMake
   - C++ compiler

4. **Check permissions** (Linux):
   - Scripts need execute permission: `chmod +x *.sh`

## Quick Reference

| Task | Linux | Windows |
|------|-------|---------|
| Install deps | `./setup_linux.sh` | `setup_windows.bat` |
| Check deps | `./check_dependencies.sh` | `check_dependencies.bat` |
| Build | `./build.sh` | `build.bat` |
| Run | `./build/VulkanApp` | `build\Release\VulkanApp.exe` |
| Clean | `rm -rf build/` | `rmdir /s build` |

## Tips for Maximum Portability

1. **Always use relative paths** in code (already done)
2. **Keep shader sources** in version control (`.spv` files are auto-generated)
3. **Use CMake** for all builds (already configured)
4. **Test on multiple platforms** before committing
5. **Document any platform-specific requirements**

Your project is now fully portable! 🚀
