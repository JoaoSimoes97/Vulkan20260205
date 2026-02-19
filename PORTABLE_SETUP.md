# Portable Development Setup

This project is portable across Windows, Linux, and macOS. See [docs/getting-started.md](docs/getting-started.md) for full setup instructions.

## What Makes It Portable

- **Automated setup scripts** — Install dependencies automatically
- **CMake build system** — Cross-platform
- **No hardcoded paths** — Everything relative
- **Vendored dependencies** — stb, TinyGLTF in deps/

## Moving to a New Machine

Transfer only these files:
```
CMakeLists.txt, vcpkg.json, setup.sh
scripts/, src/, shaders/source/, config/, levels/, deps/
```

Do NOT transfer: `build/`, `*.spv`, IDE files, `toolchain/`

Then run:
```bash
./setup.sh && scripts/linux/build.sh --debug
```

## Clean Reinstall

```bash
rm -rf build/
./setup.sh
scripts/linux/build.sh --debug
```

## More Info

See [docs/getting-started.md](docs/getting-started.md) for detailed platform-specific instructions.
