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

## Phase 4: Architecture Refactor ✅

**Goal:** Scalable architecture with performance optimizations.

### Phase 4.1: Ring-Buffered GPU Resources ✅

| Feature | Status | Notes |
|---------|--------|-------|
| RingBuffer<T> class | ✅ | Triple-buffered per-frame data |
| Persistent mapped SSBO | ✅ | GPUBuffer with persistent mapping |
| Frame-isolated light buffer | ✅ | Via FrameContextManager |
| FrameContext struct | ✅ | Per-frame resources container |

### Phase 4.2: Unified Scene System ✅

| Feature | Status | Notes |
|---------|--------|-------|
| Scene (render data) | ✅ | Object structs for GPU rendering |
| SceneNew (ECS) | ✅ | GameObjects + component pools (SoA) |
| Transform sync | ✅ | SyncTransformsToScene() copies ECS→render |
| SceneUnified (future) | ⚪ | Planned merge of Scene+SceneNew |
| BuildRenderList | ✅ | Frustum culling via BatchedDrawList |

### Phase 4.3: Renderer Extraction ✅

| Feature | Status | Notes |
|---------|--------|-------|
| Extract Renderer from VulkanApp | ✅ | renderer.h/cpp |
| RenderContext (GPU state) | ✅ | render_context.h |
| ScenePass, DebugPass, UIPass | 📋 | Planned for Phase 5 |
| DescriptorCache | ✅ | descriptor_cache.h/cpp |

### Phase 4.4: App Separation ✅

| Feature | Status | Notes |
|---------|--------|-------|
| EditorApp (Debug-only) | ✅ | editor_app.h/cpp |
| RuntimeApp (Release-only) | ✅ | runtime_app.h/cpp |
| Shared Engine core | ✅ | engine.h/cpp |
| Subsystem base class | ✅ | subsystem.h |

### Phase 4 Integration Status

The Phase 4 infrastructure classes have been created and integrated into VulkanApp:

| Component | Created | Integrated | Notes |
|-----------|---------|------------|-------|
| `FrameContextManager` | ✅ | ✅ | Initialized in VulkanApp::InitVulkan(), owns per-frame command pools |
| `RingBuffer<ObjectData>` | ✅ | ✅ | **Active** - triple-buffered SSBO, persistent mapping enabled |
| `GPUBuffer` | ✅ | ✅ | Used by RingBuffer for triple-buffered object data |
| `Scene+SceneNew` | ✅ | ✅ | **Active** - Dual architecture: Scene (render data) + SceneNew (ECS components) |
| `SceneUnified` | ✅ | ⚪ | Future: merge Scene+SceneNew into single class |
| `Renderer` | ✅ | ⚪ | Has full implementation, needs RenderContext from VulkanApp |
| `DescriptorCache` | ✅ | ✅ | **Active** - Create/ResetFrame/Destroy wired in VulkanApp |
| `Engine` | ✅ | ⚪ | Shell class, doesn't own Vulkan context yet |
| `EditorApp/RuntimeApp` | ✅ | ⚪ | Created, depend on Engine owning context |

**Completed Phase 4 Migration:**
- ✅ `STORAGE_BUFFER` → `STORAGE_BUFFER_DYNAMIC` for object data SSBO (binding 2)
- ✅ `vkMapMemory`/`vkUnmapMemory` replaced with persistent mapping via RingBuffer
- ✅ Dynamic offset passed in `vkCmdBindDescriptorSets` (one offset per frame)
- ✅ Frame index determines which ring buffer region is written and bound
- ✅ Removed single-frame `m_objectDataBuffer`/`m_objectDataMemory`
- ✅ DescriptorCache integrated (Create/ResetFrame/Destroy)

### Phase 4 Migration Technical Notes

**RingBuffer Migration (Completed ✅)**

The RingBuffer migration is complete:
1. ✅ Changed `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER` → `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC` (binding 2)
2. ✅ Updated descriptor set bindings to use ring buffer's VkBuffer
3. ✅ Pass frame offset as dynamic offset during vkCmdBindDescriptorSets
4. ✅ Removed single-frame m_objectDataBuffer/Memory
5. ✅ SSBO writes use persistent mapping (no vkMapMemory/vkUnmapMemory per frame)

**Dual Scene Architecture**

The engine uses two complementary scene representations:
- **Scene** (scene.h): Contains `Object` structs with mesh/texture handles for GPU rendering
- **SceneNew** (scene_new.h): Contains GameObjects with ECS components for editing

`SyncTransformsToScene()` copies transform changes from ECS to render objects each frame.
This design separates editing concerns (ECS) from rendering concerns (GPU data).

**Engine Migration Path (Remaining)**

For main.cpp to use Engine → EditorApp/RuntimeApp:
1. Engine must own VkInstance, VkDevice (extracted from VulkanApp)
2. Engine passes RenderContext to Renderer
3. VulkanApp becomes a compatibility shell or is removed
4. Subsystems (EditorApp, RuntimeApp) registered via Engine

**Recommended Migration Order:**
1. RenderContext population (extract Vulkan handles from VulkanApp to RenderContext)
2. Renderer integration (Renderer::Create takes populated RenderContext)
3. SceneUnified migration (replace Scene+SceneNew)
4. Engine ownership (Engine creates VkInstance/VkDevice directly)

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
| GPU Frustum Culling | ✅ | GPUCuller compute shader (verification mode) |
| Occlusion Culling | 📋 | GPU-driven culling |
| Compute Shaders | ✅ | VulkanComputePipeline class, gpu_cull.comp |
| Ray Tracing | ❌ | Blocked: No RT pipeline, no acceleration structures |
| Hybrid Rendering | ❌ | Blocked: No render graph for pass dependencies |

> **Note:** Ray Tracing/Hybrid rendering require render graph for pass dependencies.
> Compute shaders now available via VulkanComputePipeline (see GPUCuller example).

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
