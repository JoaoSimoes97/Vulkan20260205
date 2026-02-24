# Vulkan Engine Architecture

> A modular, component-based Vulkan rendering engine designed for extensibility.

---

## Table of Contents

1. [Overview](#overview)
2. [Core Architecture](#core-architecture)
3. [Module Structure](#module-structure)
4. [Entity-Component System](#entity-component-system)
5. [Rendering Pipeline](#rendering-pipeline)
6. [Resource Management](#resource-management)
7. [Threading Model](#threading-model)
8. [Extension Points](#extension-points)

---

## Overview

This engine is built around a **component-based architecture** where GameObjects are lightweight containers and functionality comes from attached components. The design prioritizes:

| Principle | Description |
|-----------|-------------|
| **Modularity** | Each system is independent and can be replaced or extended |
| **Data-Oriented Design** | Components stored in SoA for cache efficiency |
| **Composition over Inheritance** | No deep class hierarchies |
| **Async Resource Management** | Loading and cleanup on worker threads |
| **Vulkan Best Practices** | Proper synchronization, descriptor management |

---

## Core Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         VulkanApp                               │
│  (Orchestrates all systems, owns window and Vulkan stack)       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ SceneManager │  │ LightManager │  │ RenderList   │          │
│  │              │  │              │  │ Builder      │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                    Asset Managers                         │  │
│  │  ┌────────────┐  ┌─────────────┐  ┌────────────────┐     │  │
│  │  │ Mesh       │  │ Texture     │  │ Material       │     │  │
│  │  │ Manager    │  │ Manager     │  │ Manager        │     │  │
│  │  └────────────┘  └─────────────┘  └────────────────┘     │  │
│  │  ┌────────────┐  ┌─────────────┐                         │  │
│  │  │ Pipeline   │  │ Shader      │                         │  │
│  │  │ Manager    │  │ Manager     │                         │  │
│  │  └────────────┘  └─────────────┘                         │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                    Vulkan Stack                           │  │
│  │  Instance → Device → Swapchain → RenderPass → Pipelines  │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Module Structure

### Source Layout

```
src/
├── app/                    # Application entry point
│   └── vulkan_app.*        # Main orchestrator
├── core/                   # Entity-Component System
│   ├── component.h         # IComponent base interface
│   ├── gameobject.h        # GameObject container
│   ├── transform.h         # Transform component
│   ├── renderer_component.h # Rendering data
│   ├── light_component.h   # Light sources
│   ├── physics_component.h # Physics (stub)
│   ├── script_component.h  # Scripting (stub)
│   ├── camera_component.h  # Camera viewpoints
│   ├── scene_new.*         # Scene with component pools
│   ├── light_manager.*     # Light GPU management
│   └── core.h              # Aggregate header
├── managers/               # Asset management
│   ├── mesh_manager.*      # Mesh loading/caching
│   ├── texture_manager.*   # Texture loading/caching
│   ├── material_manager.*  # Material definitions
│   ├── pipeline_manager.*  # Pipeline caching
│   └── scene_manager.*     # Scene loading
├── vulkan/                 # Vulkan abstraction
│   ├── vulkan_instance.*   # Instance creation
│   ├── vulkan_device.*     # Device selection
│   ├── vulkan_swapchain.*  # Swapchain management
│   └── ...                 # Other Vulkan modules
├── render/                 # Rendering logic
│   └── render_list_builder.* # Draw call generation
├── loaders/                # Asset loaders
│   └── gltf_loader.*       # glTF file loading
└── window/                 # Windowing
    └── window.*            # SDL3 abstraction
```

---

## Entity-Component System

### GameObject

A **GameObject** is a lightweight container that holds indices into component pools. This design enables cache-efficient iteration over components of the same type.

```cpp
struct GameObject {
    uint32_t id;                  // Unique identifier
    std::string name;             // Human-readable name
    bool bActive = true;          // Active flag
    
    // Component indices (INVALID_COMPONENT_INDEX if absent)
    uint32_t transformIndex;      // Always valid
    uint32_t rendererIndex;       // Rendering data
    uint32_t lightIndex;          // Light source
    uint32_t physicsIndex;        // Physics simulation
    uint32_t scriptIndex;         // Behavior scripts
};
```

### Component Types

| Component | Purpose | Status |
|-----------|---------|--------|
| **Transform** | Position, rotation, scale, model matrix | ✅ Implemented |
| **RendererComponent** | Mesh, material, textures, visibility | ✅ Implemented |
| **LightComponent** | Point/Spot/Directional lights | ✅ Implemented |
| **CameraComponent** | Viewpoint, projection, viewport | 📋 Stub |
| **PhysicsComponent** | Rigid body, collider, forces | 📋 Stub |
| **ScriptComponent** | Lua/C++ behavior callbacks | 📋 Stub |

### SceneNew (Component Pools)

Components are stored in **Structure of Arrays** (SoA) for cache efficiency:

```cpp
class SceneNew {
    std::vector<GameObject> m_gameObjects;
    std::vector<Transform> m_transforms;
    std::vector<RendererComponent> m_renderers;
    std::vector<LightComponent> m_lights;
    // Future: physics, scripts, cameras
};
```

**Benefits:**
- Iterating all transforms is cache-friendly (contiguous memory)
- Components of same type processed together
- Enables SIMD optimization for transform updates

---

## Rendering Pipeline

### Frame Flow

```
1. Input & Camera Update
   └─ Poll events, update camera from WASD/mouse

2. Scene Update
   ├─ UpdateAllTransforms() - rebuild model matrices
   └─ LightManager.UpdateLightBuffer() - upload lights to GPU

3. Build Render List
   └─ RenderListBuilder.Build()
      ├─ Iterate RendererComponents
      ├─ Frustum culling (viewProj)
      ├─ Sort by pipeline/mesh for batching
      └─ Generate DrawCall list

4. Record Command Buffer
   ├─ Begin render pass
   ├─ For each DrawCall:
   │   ├─ Bind pipeline
   │   ├─ Bind descriptor sets (textures, SSBOs)
   │   ├─ Push constants (MVP, objectIndex)
   │   └─ Draw
   └─ End render pass

5. Submit & Present
   └─ vkQueueSubmit, vkQueuePresent
```

### Shaders

| Shader | Purpose |
|--------|---------|
| `vert.vert` | Vertex transform with instanced rendering, SSBO lookup |
| `frag.frag` | PBR lighting (Cook-Torrance) with full texture support |
| `debug_line.vert/frag` | Wireframe debug visualization |

All main shaders use the unified 96-byte push constant layout for instanced batching.

### PBR Material System

Materials use physically-based rendering:

```glsl
// From material properties SSBO
float metallic = matProps.x;
float roughness = matProps.y;
vec3 baseColor = objData.baseColor.rgb;

// Cook-Torrance BRDF
float D = DistributionGGX(N, H, roughness);
float G = GeometrySmith(N, V, L, roughness);
vec3 F = FresnelSchlick(cosTheta, F0);
```

### Descriptor Set Layout

| Binding | Type | Stage | Purpose |
|---------|------|-------|---------|
| 0 | Combined Image Sampler | Fragment | Texture |
| 1 | Uniform Buffer | Vertex + Fragment | Global uniforms |
| 2 | Storage Buffer | Vertex + Fragment | Per-object data (256B each) |
| 3 | Storage Buffer | Fragment | Light data (64B per light) |

---

## Resource Management

### Managers Overview

| Manager | Owns | Lifecycle |
|---------|------|-----------|
| **MeshManager** | VkBuffer, VkDeviceMemory | Trim on scene unload |
| **TextureManager** | VkImage, VkImageView, VkSampler | Trim on scene unload |
| **MaterialManager** | MaterialHandle (pipeline key + layout) | Trim when unused |
| **PipelineManager** | VkPipeline, VkPipelineLayout | Recreate on swapchain |
| **ShaderManager** | VkShaderModule | Trim when unused |

### Smart Pointer Lifecycle

All handles use `std::shared_ptr` with custom deleters:

```cpp
// Shader with custom deleter
std::shared_ptr<VkShaderModule> shader(
    new VkShaderModule(module),
    [device](VkShaderModule* p) { 
        vkDestroyShaderModule(device, *p, nullptr);
        delete p;
    }
);
```

**TrimUnused()** removes cache entries where `use_count() == 1`.

### Async Resource Loading

```
┌─────────────────┐    EnqueueLoad()    ┌─────────────────┐
│   Main Thread   │ ──────────────────► │  Worker Thread  │
│                 │                      │                 │
│ ProcessComplete │ ◄────────────────── │ Load from disk  │
│ (create GPU)    │   OnCompleted()     │ Parse data      │
└─────────────────┘                      └─────────────────┘
```

---

## Threading Model

### Thread Responsibilities

| Thread | Tasks |
|--------|-------|
| **Main** | Input, frame logic, command buffer recording, GPU submit |
| **Job Queue** | File I/O, asset parsing, texture decode |
| **Resource Manager** | Periodic TrimUnused(), cleanup |

### Synchronization

```cpp
// Managers use shared_mutex for read/write safety
std::shared_mutex m_mutex;

// Read operations (concurrent)
std::shared_lock lock(m_mutex);

// Write operations (exclusive)
std::unique_lock lock(m_mutex);
```

---

## Extension Points

### Adding a New Component Type

1. **Define Component** in `src/core/`:
   ```cpp
   // my_component.h
   struct MyComponent {
       // Component data
       uint32_t gameObjectIndex;
   };
   ```

2. **Add to ComponentType enum**:
   ```cpp
   enum class ComponentType {
       // ...existing...
       MyType,
   };
   ```

3. **Add pool to SceneNew**:
   ```cpp
   std::vector<MyComponent> m_myComponents;
   ```

4. **Add GameObject index**:
   ```cpp
   uint32_t myComponentIndex = INVALID_COMPONENT_INDEX;
   ```

5. **Implement Add/Remove/Get methods** in SceneNew

### Adding a New Manager

1. Create `src/managers/my_manager.h/cpp`
2. Add to `managers.h` aggregate header
3. Add member to `VulkanApp`
4. Register with `ResourceCleanupManager` if needed

### Adding a New Shader

1. Create GLSL source in `shaders/source/`
2. Run `compile_shaders.bat/sh`
3. Define pipeline key in `vulkan_pipeline.h`
4. Register layout in `VulkanApp::InitVulkan()`

---

## Build Configuration

### CMake Options

| Option | Description |
|--------|-------------|
| `CMAKE_BUILD_TYPE` | Debug (validation) or Release |
| `DEPS_STB_DIR` | Path to stb headers |
| `DEPS_TINYGLTF_DIR` | Path to TinyGLTF |

### Dependencies

| Library | Purpose | Version |
|---------|---------|---------|
| Vulkan SDK | Graphics API | 1.3+ |
| SDL3 | Windowing, input | 3.0+ |
| GLM | Math library | 0.9.9+ |
| nlohmann/json | JSON parsing | 3.11+ |
| stb | Image loading | latest |
| TinyGLTF | glTF loading | latest |

---

## Instancing & GPU Culling

The engine uses a **multi-tier instance rendering system** for optimal GPU utilization:

| Tier | Type | Update Frequency | Culling |
|------|------|------------------|---------|
| 0 | Static | Never | GPU compute |
| 1 | Semi-Static | On dirty flag | GPU compute |
| 2 | Dynamic | Per-frame | CPU |
| 3 | Procedural | Compute-driven | N/A |

For detailed architecture and implementation, see [instancing-architecture.md](instancing-architecture.md).

---

## Future Roadmap

### Immediate

- [ ] Implement Multi-Tier Instance System
- [ ] GPU culling compute pipeline
- [ ] Indirect drawing infrastructure

### Phase 2

- [ ] Shadow mapping (reuse instance buffers)
- [ ] Physics integration (Jolt or Bullet)
- [ ] Lua scripting system

### Phase 3

- [ ] Ray tracing (BLAS from static instances)
- [ ] Post-processing pipeline
- [ ] Animation/skinning

See [ROADMAP.md](ROADMAP.md) for detailed planning.
