# Vulkan Game Engine

A modular, component-based game engine built on Vulkan with PBR rendering.

**Status: Alpha (v0.1.0).** Build and run with the project scripts (see Quick Start). For known limitations and roadmap see [docs/ROADMAP.md](docs/ROADMAP.md).

## Features

- **Entity-Component System** — GameObjects with modular components (Transform, Renderer, Light, Physics, Script)
- **PBR Rendering** — Cook-Torrance BRDF with metallic/roughness workflow
- **Multi-Light System** — Point, Spot, and Directional lights (up to 256 per scene)
- **glTF 2.0 Support** — Full material loading with textures
- **Cross-Platform** — Windows, Linux, macOS (via MoltenVK)
- **Vulkan 1.3+** — Modern API with proper synchronization

## Quick Start

**Windows**
```powershell
scripts\windows\setup_windows.bat
scripts\windows\build.bat --debug
install\Debug\bin\VulkanApp.exe levels/default/level.json
```

**Linux / macOS**
```bash
./setup.sh
scripts/linux/build.sh --debug
./install/Debug/bin/VulkanApp levels/default/level.json
```
(Use only project scripts: `scripts/linux/build.sh`, `scripts/linux/clean.sh`, `scripts/linux/setup_linux.sh`; same for `scripts/windows/` and `scripts/macos/`.)

## Documentation

| Document | Description |
|----------|-------------|
| [docs/architecture.md](docs/architecture.md) | Engine architecture, ECS, rendering pipeline |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Development status and planned features |
| [docs/getting-started.md](docs/getting-started.md) | Build setup for all platforms |
| [docs/troubleshooting.md](docs/troubleshooting.md) | Common issues and solutions |

## Project Structure

```
src/
├── app/          # Application entry (VulkanApp)
├── core/         # ECS: GameObject, Transform, Components
├── managers/     # Asset managers (Mesh, Texture, Material)
├── vulkan/       # Vulkan abstraction layer
├── render/       # Draw call generation
└── loaders/      # glTF loader

shaders/source/   # GLSL shaders (PBR)
levels/           # Scene definitions (JSON)
docs/             # Documentation
```

## Architecture

The engine uses a **component-based architecture**:

- **GameObject** — Lightweight container with component indices
- **Transform** — Position, rotation, scale, cached model matrix
- **RendererComponent** — Mesh + texture + PBR material properties
- **LightComponent** — Point/Spot/Directional with intensity, range, cones
- **Scene** (unified) — SoA component pools, hierarchy, BuildRenderList for rendering

See [docs/architecture.md](docs/architecture.md) for details.

## Alpha — known limitations

- Animation and skinning: not implemented (stubs in scene manager).
- Some editor menu items are placeholders (shown in red with tooltip).
- Physics and script components: stubs only.
- See [docs/ROADMAP.md](docs/ROADMAP.md) for full status.

**Alpha validation:** Run the app with the Alpha test level (e.g. `levels/alpha_test/level.json` when available) and confirm it loads, runs at acceptable FPS, and all features (lights, materials, editor) work. See [alpha_review/questions.txt](alpha_review/questions.txt) Section 7 for what the Alpha test level must cover.

## License

Free to use and modify.
