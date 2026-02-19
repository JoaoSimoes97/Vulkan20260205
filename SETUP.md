# Quick Setup

Quick reference for setting up the project. For detailed instructions, see [docs/getting-started.md](docs/getting-started.md).

## Linux / macOS
```bash
./setup.sh
scripts/linux/build.sh --debug
./install/Debug/bin/VulkanApp levels/default/level.json
```

## Windows
```cmd
scripts\windows\setup_windows.bat
scripts\windows\build.bat --debug
install\Debug\bin\VulkanApp.exe levels/default/level.json
```

## More Info

- [docs/getting-started.md](docs/getting-started.md) — Full setup guide
- [docs/ROADMAP.md](docs/ROADMAP.md) — Feature status
- [docs/troubleshooting.md](docs/troubleshooting.md) — Common issues

If setup fails, check:
1. Internet connection (for package downloads)
2. Administrator/sudo permissions
3. Run `check_dependencies.sh` or `check_dependencies.bat` to see what's missing

For detailed instructions, see [README.md](README.md).
