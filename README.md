# Vulkan Game Engine

A modular, component-based game engine built on Vulkan with PBR rendering.

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
build\Debug\VulkanApp.exe levels/default/level.json
```

**Linux / macOS**
```bash
./setup.sh
scripts/linux/build.sh --debug
./build/Debug/VulkanApp levels/default/level.json
```

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
- **SceneNew** — SoA component pools for cache efficiency

See [docs/architecture.md](docs/architecture.md) for details.

## License

Free to use and modify.
