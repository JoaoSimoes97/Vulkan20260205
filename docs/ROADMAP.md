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
| Cameras Panel | ✅ | Add/delete/edit scene cameras |
| Runtime Stats Overlay | ✅ | Draw calls, triangles, culling % |
| Undo/Redo System | 📋 | Command pattern |
| Play/Pause/Stop | 📋 | Runtime control |

---

## Phase 4: Architecture Refactor 🔄

**Goal:** Scalable architecture with performance optimizations.

### Phase 4.1: Ring-Buffered GPU Resources 📋

| Feature | Status | Notes |
|---------|--------|-------|
| RingBuffer<T> class | 📋 | Triple-buffered per-frame data |
| Persistent mapped SSBO | 📋 | No vkMapMemory per frame |
| Frame-isolated light buffer | 📋 | Eliminate GPU race conditions |
| FrameContext struct | 📋 | Per-frame resources container |

### Phase 4.2: Unified Scene System 📋

| Feature | Status | Notes |
|---------|--------|-------|
| Merge Scene + SceneNew | 📋 | Single ECS-based scene |
| Remove legacy Scene sync | 📋 | Eliminate SyncFromSceneNew() |
| Component-only architecture | 📋 | All data in pools |
| Update systems (Transform, Light) | 📋 | Process components in batches |

### Phase 4.3: Renderer Extraction 📋

| Feature | Status | Notes |
|---------|--------|-------|
| Extract Renderer from VulkanApp | 📋 | VulkanApp → 800 lines max |
| RenderContext (GPU state) | 📋 | Device, queues, pools |
| ScenePass, DebugPass, UIPass | 📋 | Separate render pass classes |
| DescriptorCache | 📋 | Pre-allocated descriptor pool |

### Phase 4.4: App Separation 📋

| Feature | Status | Notes |
|---------|--------|-------|
| EditorApp (Debug-only) | 📋 | Viewports, panels, gizmos |
| RuntimeApp (Release-only) | 📋 | Minimal runtime loop |
| Shared Engine core | 📋 | Scene, Renderer, Input |
| Subsystem base class | 📋 | Init/Update/Shutdown lifecycle |

### Target Architecture

```
src/
├── core/                    # Engine core (no Vulkan specifics)
│   ├── engine.h/cpp         # Main loop, subsystem coordination
│   ├── subsystem.h          # Base class for subsystems
│   └── frame_context.h      # Per-frame data
│
├── scene/                   # Unified ECS scene
│   ├── scene.h/cpp          # Single scene class
│   ├── components/          # Transform, Mesh, Light, Camera
│   └── systems/             # TransformSystem, LightSystem
│
├── render/
│   ├── renderer.h/cpp       # High-level orchestration
│   ├── render_context.h     # GPU resources
│   ├── resources/           # gpu_buffer, descriptor_cache
│   └── passes/              # scene_pass, debug_pass, ui_pass
│
├── platform/                # Window, input, Vulkan instance
└── app/
    ├── editor_app.h/cpp     # Editor (Debug)
    └── runtime_app.h/cpp    # Runtime (Release)
```

---

## Phase 5: Streaming System 📋

**Goal:** Dynamic loading/unloading of world sectors for large/procedural maps.

### Phase 5.1: Core Infrastructure 📋

| Feature | Status | Notes |
|---------|--------|-------|
| ObjectPool (slot recycling) | 📋 | O(1) alloc/free for SSBO indices |
| IncrementalBatchList | 📋 | Add/remove without full rebuild |
| SpatialIndex (BVH/Octree) | 📋 | Fast culling + streaming queries |

### Phase 5.2: Sector System 📋

| Feature | Status | Notes |
|---------|--------|-------|
| Sector definition | 📋 | Spatial unit with bounds + assets |
| SectorLoader (async) | 📋 | Background loading via JobQueue |
| StreamManager | 📋 | Distance-based load/unload |
| Load/Unload hysteresis | 📋 | Prevent thrashing |

### Phase 5.3: LOD & Optimization 📋

| Feature | Status | Notes |
|---------|--------|-------|
| Per-sector LOD levels | 📋 | Distance-based mesh switching |
| Streaming budget | 📋 | Memory/bandwidth limits |
| Priority queue | 📋 | Camera direction + visibility |
| Procedural sector support | 📋 | Runtime-generated content |

### Level File Format

```json
{
    "streaming": { "loadRadius": 100, "unloadRadius": 150 },
    "sectors": [
        { "id": 0, "path": "sectors/0_0.glb", "bounds": {...} }
    ]
}
```

---

## Phase 6: Scripting & Physics 📋

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

## Phase 7: Advanced Rendering 📋

**Goal:** Visual fidelity and performance features.

| Feature | Status | Notes |
|---------|--------|-------|
| Shadow Mapping | 📋 | Directional + Spot |
| Point Light Shadows | 📋 | Cubemap shadows |
| Post-Processing | 📋 | Bloom, tone mapping |
| MSAA | 📋 | Multisample anti-aliasing |
| Animation/Skinning | 📋 | glTF animation support |
| Instanced Rendering | ✅ | BatchedDrawList with dirty tracking |
| Occlusion Culling | 📋 | GPU-driven culling |

---

## Phase 8: Platform & Distribution 📋

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
| Dual scene system (Scene + SceneNew) | Medium | Phase 4.2 will unify |
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
