# Development Roadmap

> Current status and planned features for the Vulkan Engine.

**Last Updated:** February 2026

---

## Status Legend

| Icon | Status |
|------|--------|
| ✅ | Completed |
| 🔄 | In Progress |
| 📋 | Planned |
| ❌ | Blocked/On Hold |

---

## Phase 1: Core Foundation ✅

**Goal:** Establish basic Vulkan rendering with asset loading.

| Feature | Status | Notes |
|---------|--------|-------|
| SDL3 Window + Vulkan Surface | ✅ | Cross-platform |
| Vulkan Instance + Device | ✅ | Validation layers in Debug |
| Swapchain + Render Pass | ✅ | Dynamic resize support |
| Basic Vertex/Fragment Shaders | ✅ | SPIR-V compilation |
| Mesh Loading (glTF) | ✅ | Via TinyGLTF |
| Texture Loading | ✅ | Via stb_image |
| Camera System | ✅ | WASD + mouse look |
| Push Constants | ✅ | MVP + objectIndex |
| Descriptor Sets | ✅ | Texture + UBO + SSBO |

---

## Phase 2: Entity-Component System ✅

**Goal:** Modular GameObject architecture with component pools.

| Feature | Status | Notes |
|---------|--------|-------|
| GameObject Container | ✅ | Lightweight with indices |
| Transform Component | ✅ | Position/rotation/scale + matrix |
| RendererComponent | ✅ | Mesh + texture + material props |
| LightComponent | ✅ | Point/Spot/Directional |
| SceneNew (SoA) | ✅ | Component pools for cache efficiency |
| LightManager | ✅ | GPU buffer management |
| PBR Shaders | ✅ | Cook-Torrance BRDF |
| Multi-Light Support | ✅ | Up to 256 lights via SSBO |

---

## Phase 3: Editor & Debug Tools ✅

**Goal:** Visual debugging and runtime editing capabilities.

| Feature | Status | Notes |
|---------|--------|-------|
| Light Debug Renderer | ✅ | Wireframe spheres/cones/arrows for lights |
| Config System | ✅ | JSON config with auto-creation |
| ImGui Integration | ✅ | Editor overlays |
| Selection System | ✅ | Click-to-select + hierarchy selection |
| Scene Hierarchy Panel | ✅ | GameObject tree |
| Inspector Panel | ✅ | Transform, Light, Renderer components |
| Gizmos (Transform) | ✅ | Move/Rotate/Scale handles via ImGuizmo |
| Multi-Viewport | ✅ | Docked viewports with camera-per-viewport |
| CameraComponent | ✅ | Scene cameras with perspective/ortho |
| Mesh/Material Inspector | ✅ | Vertex count, AABB, PBR properties |
| Undo/Redo System | 📋 | Command pattern |
| Play/Pause/Stop | 📋 | Runtime control |

---

## Phase 4: Scripting & Physics 📋

**Goal:** Dynamic behavior and physics simulation.

| Feature | Status | Notes |
|---------|--------|-------|
| Lua Scripting | 📋 | Via LuaJIT or sol2 |
| Native Script Callbacks | 📋 | C++ function binding |
| Physics Engine | 📋 | Jolt or Bullet |
| Collision Detection | 📋 | Sphere/Box/Capsule |
| Rigid Body Dynamics | 📋 | Forces, velocity |
| Trigger Volumes | 📋 | Event callbacks |

---

## Phase 5: Advanced Rendering 📋

**Goal:** Visual fidelity and performance features.

| Feature | Status | Notes |
|---------|--------|-------|
| Shadow Mapping | 📋 | Directional + Spot |
| Point Light Shadows | 📋 | Cubemap shadows |
| Post-Processing | 📋 | Bloom, tone mapping |
| MSAA | 📋 | Multisample anti-aliasing |
| Animation/Skinning | 📋 | glTF animation support |
| Instanced Rendering | 📋 | Indirect draw buffers |
| Occlusion Culling | 📋 | GPU-driven culling |

---

## Phase 6: Platform & Distribution 📋

**Goal:** Cross-platform deployment.

| Feature | Status | Notes |
|---------|--------|-------|
| Windows Build | ✅ | Primary platform |
| Linux Build | ✅ | Tested on Ubuntu |
| macOS Build | 📋 | MoltenVK required |
| Android Build | 📋 | Via NDK |
| iOS Build | 📋 | Via MoltenVK |
| Asset Bundling | 📋 | Compressed archives |

---

## Known Issues

| Issue | Priority | Status |
|-------|----------|--------|
| PDB lock during parallel builds | Low | Workaround: single-thread build |
| Dual scene system (Scene + SceneNew) | Low | Both working; full migration when ECS complete |
| Animation import not implemented | Low | Logged when glTF has animations |
| Skinning import not implemented | Low | Logged when glTF has skins |
| Cylinder/Cone mesh caps missing | Low | Wireframe only for now |

---

## Contributing

See [guidelines/coding-guidelines.md](guidelines/coding-guidelines.md) for code style.

When adding features:
1. Check this roadmap for planned items
2. Discuss major changes before implementation
3. Update docs when completing features
4. Mark status in this file when done
