# Third-party dependencies (no download during build)

This directory is **populated by the project setup scripts** so that CMake never downloads anything during build.

## What goes here

| Directory   | Source | Populated by |
|------------|--------|--------------|
| `stb/`     | [nothings/stb](https://github.com/nothings/stb) | `./setup.sh` or `scripts/linux/setup_linux.sh` (and macOS/Windows setup) |
| `tinygltf/`| [syoyo/tinygltf](https://github.com/syoyo/tinygltf) (v2.9.7) | Same |

## If you didn't run setup

Run the setup script for your platform first:

- **Linux / macOS:** `./setup.sh` or `scripts/linux/setup_linux.sh` / `scripts/macos/setup_macos.sh`
- **Windows:** `scripts\windows\setup_windows.bat`

Setup installs system packages (Vulkan, SDL3, nlohmann-json, etc.) and clones `stb` and `tinygltf` into `deps/`. After that, build with CMake only (no network).

## Manual clone (no git in setup)

If you prefer not to use git during setup or setup didn't clone:

```bash
mkdir -p deps
git clone --depth 1 https://github.com/nothings/stb.git deps/stb
git clone --depth 1 --branch v2.9.7 https://github.com/syoyo/tinygltf.git deps/tinygltf
```

On Windows (PowerShell or cmd):

```bat
mkdir deps
git clone --depth 1 https://github.com/nothings/stb.git deps\stb
git clone --depth 1 --branch v2.9.7 https://github.com/syoyo/tinygltf.git deps\tinygltf
```

Then configure and build as usual; CMake will use `deps/` and will not fetch anything.
