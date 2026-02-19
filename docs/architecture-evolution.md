# Architecture Evolution Discussion

> **Purpose:** Evaluate current architecture and plan improvements before continuing feature development.  
> **Status:** Under Discussion  
> **Date:** February 2026

---

## Current Pain Points

| Issue | Where | Why It's Bad |
|-------|-------|--------------|
| Hardcoded buffer sizes | `vulkan_app.cpp`, `light_manager.cpp` | Can't scale dynamically |
| Single pipeline type | `PipelineManager` | Only graphics, no compute/raytracing |
| Rendering tied to VulkanApp | `vulkan_app.cpp` | 1000+ line monolith |
| No game logic layer | N/A | Engine and game are the same |
| No abstraction over Vulkan | Direct API calls everywhere | Hard to port, hard to test |

---

## Goals

1. **Dynamic Resource Sizing** — Buffers grow/shrink based on usage
2. **Multi-Pipeline Support** — Graphics, Compute, Ray Tracing
3. **Modular Subsystems** — Replaceable, testable modules
4. **Game/Engine Separation** — Clean API between engine and game code
5. **Render Graph** — Declarative rendering with automatic synchronization
6. **Data-Driven Configuration** — No magic numbers in code

---

## Proposed Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                           GAME LAYER                                 │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐   │
│  │ GameState   │ │ Player      │ │ AI Systems  │ │ Game UI    │   │
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘   │
│                              │                                       │
│                              ▼                                       │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                      ENGINE API                                │  │
│  │  - CreateObject(params)     - SetTransform(id, pos, rot)      │  │
│  │  - DestroyObject(id)        - AddComponent<T>(id)             │  │
│  │  - LoadScene(path)          - GetComponent<T>(id)             │  │
│  │  - PlaySound(id)            - RayCast(origin, dir)            │  │
│  └───────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          ENGINE CORE                                 │
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │                    SUBSYSTEM MANAGER                         │    │
│  │  Owns and updates all subsystems in correct order            │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                │                                     │
│       ┌────────────┬───────────┼───────────┬────────────┐           │
│       ▼            ▼           ▼           ▼            ▼           │
│  ┌─────────┐ ┌──────────┐ ┌─────────┐ ┌─────────┐ ┌──────────┐     │
│  │ Scene   │ │ Physics  │ │ Audio   │ │ Input   │ │ Scripting│     │
│  │ System  │ │ System   │ │ System  │ │ System  │ │ System   │     │
│  └─────────┘ └──────────┘ └─────────┘ └─────────┘ └──────────┘     │
│       │                                                              │
│       ▼                                                              │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │                     RENDER SYSTEM                            │    │
│  │  ┌───────────────┐ ┌───────────────┐ ┌───────────────┐      │    │
│  │  │ Render Graph  │ │ Material Sys  │ │ Light System  │      │    │
│  │  └───────────────┘ └───────────────┘ └───────────────┘      │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                │                                     │
│                                ▼                                     │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │                    RENDER BACKEND                            │    │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐            │    │
│  │  │ RHI Layer   │ │ GPU Memory  │ │ Pipeline    │            │    │
│  │  │ (Abstract)  │ │ Allocator   │ │ Cache       │            │    │
│  │  └─────────────┘ └─────────────┘ └─────────────┘            │    │
│  │         │                                                    │    │
│  │         ▼                                                    │    │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐            │    │
│  │  │ Vulkan Impl │ │ D3D12 Impl  │ │ Metal Impl  │            │    │
│  │  │ (Current)   │ │ (Future)    │ │ (Future)    │            │    │
│  │  └─────────────┘ └─────────────┘ └─────────────┘            │    │
│  └─────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 1. Dynamic Buffer Management

### Problem
```cpp
// Current: Hardcoded everywhere
constexpr uint32_t MAX_OBJECTS = 1024;
constexpr uint32_t MAX_LIGHTS = 64;
constexpr uint32_t OBJECT_DATA_SIZE = 256;
```

### Solution: GPU Buffer Allocator

```cpp
// Proposed: Dynamic GPU buffer with growth
class DynamicGPUBuffer {
public:
    struct Config {
        VkBufferUsageFlags usage;
        VkMemoryPropertyFlags memoryFlags;
        size_t initialCapacity;
        size_t growthFactor = 2;      // Double on resize
        size_t maxCapacity = 0;       // 0 = unlimited
        size_t alignment = 256;       // For SSBO/UBO alignment
    };
    
    // Create with initial capacity
    bool Create(VkDevice device, VkPhysicalDevice physDevice, const Config& config);
    
    // Ensure capacity, returns true if resized
    bool EnsureCapacity(size_t requiredBytes);
    
    // Write data, handles dirty tracking
    void Write(size_t offset, const void* data, size_t size);
    
    // Flush only dirty regions to GPU
    void FlushDirty();
    
    // Get current stats
    size_t GetCapacity() const;
    size_t GetUsedBytes() const;
    VkBuffer GetBuffer() const;
    
private:
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    void* m_mappedPtr = nullptr;
    
    // Dirty tracking
    std::vector<std::pair<size_t, size_t>> m_dirtyRanges;
    
    // Ring buffer for per-frame isolation
    uint32_t m_frameIndex = 0;
    uint32_t m_frameCount = 3;
};
```

### Config-Driven Limits

```json
// config/engine.json
{
  "gpu": {
    "initial_object_buffer_size": 65536,
    "initial_light_buffer_size": 4096,
    "max_object_buffer_size": 16777216,
    "max_light_buffer_size": 262144,
    "buffer_growth_factor": 2,
    "ring_buffer_frames": 3
  }
}
```

---

## 2. Multi-Pipeline Architecture

### Problem
```cpp
// Current: Only graphics pipelines
class PipelineManager {
    VkPipeline CreateGraphicsPipeline(...);  // Only this exists
};
```

### Solution: Pipeline Registry with Types

```cpp
enum class PipelineType {
    Graphics,
    Compute,
    RayTracing
};

// Abstract pipeline description
struct PipelineDesc {
    PipelineType type;
    std::string name;
    std::vector<std::string> shaderPaths;
    
    // Type-specific config (variant)
    struct GraphicsConfig {
        VkPrimitiveTopology topology;
        VkPolygonMode polygonMode;
        VkCullModeFlags cullMode;
        bool depthTest;
        bool depthWrite;
        bool blendEnable;
        // ...
    };
    
    struct ComputeConfig {
        uint32_t localSizeX, localSizeY, localSizeZ;
    };
    
    struct RayTracingConfig {
        uint32_t maxRecursionDepth;
        // ...
    };
    
    std::variant<GraphicsConfig, ComputeConfig, RayTracingConfig> config;
};

// Pipeline registry
class PipelineRegistry {
public:
    // Register from JSON or code
    PipelineHandle Register(const PipelineDesc& desc);
    PipelineHandle GetByName(const std::string& name);
    
    // Bind pipeline for rendering
    void Bind(VkCommandBuffer cmd, PipelineHandle handle);
    
    // Hot-reload shaders (debug mode)
    void ReloadShader(const std::string& path);
    
private:
    std::map<std::string, PipelineHandle> m_pipelines;
    std::map<PipelineHandle, VkPipeline> m_vkPipelines;
};
```

### Data-Driven Pipeline Definitions

```json
// config/pipelines/pbr_opaque.json
{
  "name": "pbr_opaque",
  "type": "graphics",
  "shaders": {
    "vertex": "shaders/pbr.vert.spv",
    "fragment": "shaders/pbr.frag.spv"
  },
  "graphics": {
    "topology": "TRIANGLE_LIST",
    "polygon_mode": "FILL",
    "cull_mode": "BACK",
    "depth_test": true,
    "depth_write": true,
    "blend_enable": false
  },
  "descriptor_sets": ["global", "material", "object"]
}
```

```json
// config/pipelines/light_cull.json
{
  "name": "light_cull",
  "type": "compute",
  "shaders": {
    "compute": "shaders/light_cull.comp.spv"
  },
  "compute": {
    "local_size": [16, 16, 1]
  },
  "descriptor_sets": ["lights", "tiles"]
}
```

---

## 3. Subsystem Architecture

### Problem
```cpp
// Current: Everything in VulkanApp
class VulkanApp {
    // 1000+ lines mixing:
    // - Window management
    // - Vulkan setup
    // - Scene management
    // - Rendering
    // - Input handling
    // - Resource loading
};
```

### Solution: Modular Subsystems

```cpp
// Base interface for all subsystems
class ISubsystem {
public:
    virtual ~ISubsystem() = default;
    
    virtual const char* GetName() const = 0;
    virtual int GetPriority() const = 0;  // Update order
    
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(float deltaTime) = 0;
    
    // Optional hooks
    virtual void OnSceneLoad() {}
    virtual void OnSceneUnload() {}
    virtual void OnWindowResize(uint32_t width, uint32_t height) {}
};

// Subsystem manager owns and orchestrates subsystems
class SubsystemManager {
public:
    template<typename T, typename... Args>
    T* AddSubsystem(Args&&... args);
    
    template<typename T>
    T* GetSubsystem();
    
    void InitializeAll();
    void ShutdownAll();
    void UpdateAll(float deltaTime);
    
private:
    std::vector<std::unique_ptr<ISubsystem>> m_subsystems;
    std::map<std::type_index, ISubsystem*> m_typeMap;
};

// Example subsystems
class WindowSubsystem : public ISubsystem { ... };
class InputSubsystem : public ISubsystem { ... };
class SceneSubsystem : public ISubsystem { ... };
class PhysicsSubsystem : public ISubsystem { ... };
class AudioSubsystem : public ISubsystem { ... };
class RenderSubsystem : public ISubsystem { ... };
class ScriptSubsystem : public ISubsystem { ... };
```

### Dependency Injection

```cpp
// Subsystems can request dependencies
class RenderSubsystem : public ISubsystem {
public:
    void Initialize() override {
        m_pScene = m_pSubsystemManager->GetSubsystem<SceneSubsystem>();
        m_pWindow = m_pSubsystemManager->GetSubsystem<WindowSubsystem>();
    }
    
private:
    SceneSubsystem* m_pScene = nullptr;
    WindowSubsystem* m_pWindow = nullptr;
};
```

---

## 4. Game/Engine Separation

### Problem
```cpp
// Current: No separation
// main.cpp directly creates VulkanApp and calls Run()
// No place to put game-specific logic
```

### Solution: Engine API + Game Application

```cpp
// Engine provides abstract Application class
class Application {
public:
    virtual ~Application() = default;
    
    // Game overrides these
    virtual void OnCreate() {}
    virtual void OnDestroy() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnFixedUpdate(float fixedDeltaTime) {}
    virtual void OnRender() {}
    virtual void OnGUI() {}
    
protected:
    // Engine API available to game
    Engine* GetEngine() { return m_pEngine; }
    SceneAPI* GetScene() { return m_pEngine->GetSceneAPI(); }
    InputAPI* GetInput() { return m_pEngine->GetInputAPI(); }
    AudioAPI* GetAudio() { return m_pEngine->GetAudioAPI(); }
    PhysicsAPI* GetPhysics() { return m_pEngine->GetPhysicsAPI(); }
    
private:
    Engine* m_pEngine = nullptr;
};

// Game implements Application
class MyGame : public Application {
public:
    void OnCreate() override {
        // Load initial scene
        GetScene()->LoadScene("levels/main.json");
        
        // Create player
        m_playerId = GetScene()->CreateObject("Player");
        GetScene()->AddComponent<RendererComponent>(m_playerId);
        GetScene()->AddComponent<RigidBody>(m_playerId);
    }
    
    void OnUpdate(float dt) override {
        // Game logic
        if (GetInput()->IsKeyDown(Key::W)) {
            auto& transform = GetScene()->GetTransform(m_playerId);
            transform.position.z -= m_moveSpeed * dt;
        }
    }
    
private:
    ObjectID m_playerId;
    float m_moveSpeed = 5.0f;
};

// Entry point
int main() {
    Engine engine;
    engine.Run<MyGame>();
    return 0;
}
```

### Clean Engine API

```cpp
// Scene API (what game code sees)
class SceneAPI {
public:
    // Object management
    ObjectID CreateObject(const char* name = nullptr);
    void DestroyObject(ObjectID id);
    bool IsValid(ObjectID id) const;
    
    // Transform
    Transform& GetTransform(ObjectID id);
    void SetWorldPosition(ObjectID id, const glm::vec3& pos);
    void SetWorldRotation(ObjectID id, const glm::quat& rot);
    glm::vec3 GetWorldPosition(ObjectID id) const;
    
    // Components
    template<typename T>
    T* AddComponent(ObjectID id);
    
    template<typename T>
    T* GetComponent(ObjectID id);
    
    template<typename T>
    bool HasComponent(ObjectID id) const;
    
    template<typename T>
    void RemoveComponent(ObjectID id);
    
    // Scene loading
    void LoadScene(const char* path);
    void UnloadScene();
    
    // Queries
    ObjectID FindByName(const char* name) const;
    std::vector<ObjectID> FindByTag(const char* tag) const;
    std::vector<ObjectID> FindInRadius(const glm::vec3& center, float radius) const;
    
    // Ray casting
    struct RayHit {
        ObjectID objectId;
        glm::vec3 point;
        glm::vec3 normal;
        float distance;
    };
    bool RayCast(const glm::vec3& origin, const glm::vec3& dir, float maxDist, RayHit& outHit);
};
```

---

## 5. Render Graph (Future)

### Problem
```cpp
// Current: Fixed render pass order hardcoded in Record()
void VulkanCommandBuffers::Record(...) {
    vkCmdBeginRenderPass(...);
    // Draw everything
    vkCmdEndRenderPass(...);
}
```

### Solution: Declarative Render Graph

```cpp
// Describe what to render, not how
class RenderGraph {
public:
    // Define resources
    ResourceHandle AddTransientTexture(const char* name, TextureDesc desc);
    ResourceHandle AddExternalTexture(const char* name, VkImageView view);
    ResourceHandle AddBuffer(const char* name, BufferDesc desc);
    
    // Define passes
    PassHandle AddGraphicsPass(const char* name, GraphicsPassDesc desc,
        std::function<void(RenderContext&, PassResources&)> execute);
    
    PassHandle AddComputePass(const char* name, ComputePassDesc desc,
        std::function<void(ComputeContext&, PassResources&)> execute);
    
    // Compile and execute
    void Compile();  // Builds dependency graph, inserts barriers
    void Execute(VkCommandBuffer cmd);
    
private:
    // Automatic barrier insertion based on resource usage
    void InsertBarriers();
};

// Usage example
void RenderSystem::BuildGraph() {
    auto& graph = m_renderGraph;
    
    // G-Buffer pass
    auto gbufferAlbedo = graph.AddTransientTexture("GBuffer_Albedo", {width, height, VK_FORMAT_R8G8B8A8_UNORM});
    auto gbufferNormal = graph.AddTransientTexture("GBuffer_Normal", {width, height, VK_FORMAT_R16G16B16A16_SFLOAT});
    auto gbufferDepth = graph.AddTransientTexture("GBuffer_Depth", {width, height, VK_FORMAT_D32_SFLOAT});
    
    graph.AddGraphicsPass("GBuffer", {
        .colorAttachments = {gbufferAlbedo, gbufferNormal},
        .depthAttachment = gbufferDepth,
    }, [this](RenderContext& ctx, PassResources& res) {
        ctx.BindPipeline("gbuffer");
        for (auto& obj : m_opaqueObjects) {
            ctx.Draw(obj);
        }
    });
    
    // Light culling (compute)
    auto lightTiles = graph.AddBuffer("LightTiles", {tileCount * 256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT});
    
    graph.AddComputePass("LightCull", {
        .reads = {gbufferDepth, m_lightBuffer},
        .writes = {lightTiles},
    }, [this](ComputeContext& ctx, PassResources& res) {
        ctx.BindPipeline("light_cull");
        ctx.Dispatch(tileCountX, tileCountY, 1);
    });
    
    // Lighting pass
    auto hdrBuffer = graph.AddTransientTexture("HDR", {width, height, VK_FORMAT_R16G16B16A16_SFLOAT});
    
    graph.AddGraphicsPass("Lighting", {
        .colorAttachments = {hdrBuffer},
        .reads = {gbufferAlbedo, gbufferNormal, gbufferDepth, lightTiles},
    }, [this](RenderContext& ctx, PassResources& res) {
        ctx.BindPipeline("deferred_lighting");
        ctx.DrawFullscreenQuad();
    });
    
    // Tone mapping + output
    graph.AddGraphicsPass("Tonemap", {
        .colorAttachments = {m_swapchainImage},
        .reads = {hdrBuffer},
    }, [this](RenderContext& ctx, PassResources& res) {
        ctx.BindPipeline("tonemap");
        ctx.DrawFullscreenQuad();
    });
    
    graph.Compile();
}
```

---

## 6. File Structure Proposal

```
src/
├── engine/
│   ├── core/
│   │   ├── engine.h/cpp           # Main engine class
│   │   ├── application.h          # Base application class
│   │   ├── subsystem.h            # Subsystem interface
│   │   ├── subsystem_manager.h/cpp
│   │   └── types.h                # ObjectID, HandleTypes, etc.
│   │
│   ├── ecs/
│   │   ├── scene.h/cpp            # Scene (replaces SceneNew)
│   │   ├── component_pool.h       # Generic component pool
│   │   ├── transform.h            # Transform component
│   │   └── components/
│   │       ├── renderer.h
│   │       ├── light.h
│   │       ├── rigidbody.h
│   │       └── script.h
│   │
│   ├── render/
│   │   ├── render_system.h/cpp    # Render subsystem
│   │   ├── render_graph.h/cpp     # Declarative render graph
│   │   ├── pipeline_registry.h/cpp
│   │   ├── material_system.h/cpp
│   │   └── debug/
│   │       ├── debug_draw.h/cpp   # Debug lines, shapes
│   │       └── imgui_layer.h/cpp  # Editor UI
│   │
│   ├── rhi/
│   │   ├── rhi.h                  # Abstract RHI interface
│   │   ├── rhi_types.h            # Platform-agnostic types
│   │   ├── gpu_buffer.h/cpp       # Dynamic GPU buffers
│   │   └── vulkan/
│   │       ├── vulkan_rhi.h/cpp   # Vulkan implementation
│   │       ├── vulkan_device.h/cpp
│   │       ├── vulkan_swapchain.h/cpp
│   │       └── ...
│   │
│   ├── resource/
│   │   ├── resource_manager.h/cpp # Unified resource loading
│   │   ├── mesh_loader.h/cpp
│   │   ├── texture_loader.h/cpp
│   │   └── shader_loader.h/cpp
│   │
│   ├── physics/
│   │   ├── physics_system.h/cpp
│   │   └── collision.h/cpp
│   │
│   ├── audio/
│   │   └── audio_system.h/cpp
│   │
│   ├── input/
│   │   └── input_system.h/cpp
│   │
│   └── script/
│       ├── script_system.h/cpp
│       └── lua_bindings.h/cpp
│
├── game/                          # Game-specific code
│   ├── game.h/cpp                 # MyGame : Application
│   ├── player/
│   ├── enemies/
│   └── ui/
│
└── main.cpp                       # Entry point
```

---

## Implementation Phases

### Phase 0: Foundation Refactor (Before Continuing)

| Task | Effort | Impact |
|------|--------|--------|
| Create `DynamicGPUBuffer` class | 1 day | Removes hardcoded sizes |
| Move constants to config JSON | 0.5 day | Data-driven limits |
| Extract `SubsystemManager` | 1 day | Modular architecture |
| Split VulkanApp into subsystems | 2 days | Clean separation |
| Create `Application` base class | 0.5 day | Game/engine split |

**Total: ~5 days**

### Phase 1-6: Continue as Planned

With foundation in place, continue with:
- Editor & Debug Tools
- Scripting & Physics
- Advanced Rendering
- Platform & Distribution

---

## Questions for Discussion

1. **RHI Abstraction Level**
   - Full abstraction (could port to D3D12/Metal) — More work, more portable
   - Vulkan-only with clean layers — Less work, still clean

2. **ECS Implementation**
   - Current SoA pools — Fast, but fixed component types
   - Full ECS (entt, flecs) — More flexible, slight overhead
   - Hybrid — SoA for hot components, map for rare ones

3. **Render Graph**
   - Implement now — Clean from start, but complex
   - Implement later — Ship faster, refactor later

4. **Scripting Language**
   - Lua — Fast, simple, proven (Unity, Roblox)
   - C# — More familiar, heavier (Godot, Unity)
   - Native C++ only — No runtime overhead, less flexible

5. **Priority**
   - Performance first, features later?
   - Features first, optimize later?
   - Clean architecture first, both follow?

---

## My Recommendation

**Start with Phase 0: Foundation Refactor**

1. **Day 1:** `DynamicGPUBuffer` + config-driven limits
2. **Day 2:** `SubsystemManager` framework
3. **Day 3-4:** Split VulkanApp into subsystems
4. **Day 5:** `Application` base class + game separation

Then continue with editor features on a clean foundation.

---

## Next Steps

After we agree on direction:
1. Create detailed task breakdown
2. Update ROADMAP.md with Phase 0
3. Begin implementation

**What are your thoughts on:**
- Priority order?
- RHI abstraction level?
- Anything to add or remove?
