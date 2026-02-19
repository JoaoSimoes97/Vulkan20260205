# Vulkan Engine Architecture - Full System Design

This document describes the complete engine architecture: objects, components, physics, scripting, data layout, and threading model.

---

## 1. Core Architectural Principles

| Principle | Implication |
|-----------|-----------|
| **Component-Based Architecture** | GameObjects are containers; functionality comes from components (Renderer, Physics, Script) |
| **Composition over Inheritance** | No deep class hierarchies; use component composition for flexibility |
| **Data-Oriented Design** | Components stored in arrays for cache efficiency; loose coupling between systems |
| **Async Resource Management** | Resource cleanup happens on worker thread; main thread never blocked |
| **Frame Pipelining** | Physics → Script → Render → GPU; each stage independent and overlappable |

---

## 2. GameObject and Components

### GameObject Structure

A **GameObject** is a container with:
- **Unique ID** — for finding objects in scene
- **Name** — human-readable identifier (optional)
- **Active Flag** — can be disabled without removing from scene
- **Transaction Component** — position, rotation, scale (always present)
- **Optional Components** — Renderer, Physics, Script, custom

```cpp
struct GameObject {
    uint32_t id;                          // Unique ID
    std::string name;                     // Human-readable name
    bool bActive = true;
    glm::vec3 position = glm::vec3(0);   // Local position
    glm::quat rotation = glm::quat(1,0,0,0);  // Local rotation
    glm::vec3 scale = glm::vec3(1);      // Local scale
    
    // Component presence flags
    bool bHasRenderer = false;
    bool bHasPhysics = false;
    bool bHasScript = false;
    
    // Component indices (into component pools)
    uint32_t rendererIndex = UINT32_MAX;
    uint32_t physicsIndex = UINT32_MAX;
    uint32_t scriptIndex = UINT32_MAX;
};
```

### Component Types

#### 1. **Renderer Component**
Describes how to draw this object.

```cpp
struct RendererComponent {
    std::shared_ptr<MeshHandle> mesh;
    std::shared_ptr<MaterialHandle> material;
    uint32_t layer = 0;            // For sorting/culling
    bool bCastShadow = true;
    bool bReceiveShadow = true;
    glm::vec4 tintColor = glm::vec4(1);
};
```

**Responsibilities:**
- Hold references to mesh and material
- Store rendering state (tint color, layer, shadow flags)
- Read by RenderListBuilder to generate DrawCalls

#### 2. **Physics Component**
Simulates physics: collisions, forces, velocity.

```cpp
struct RigidBody {
    enum Type { Static, Dynamic, Kinematic };
    Type type = Type::Dynamic;
    
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.5f;
    glm::vec3 linearVelocity = glm::vec3(0);
    glm::vec3 angularVelocity = glm::vec3(0);
    glm::vec3 accumForce = glm::vec3(0);
    glm::vec3 accumTorque = glm::vec3(0);
};

struct Collider {
    enum Shape { Sphere, Box, Capsule };
    Shape shape = Shape::Sphere;
    float radius = 0.5f;
    glm::vec3 extents = glm::vec3(1);  // For box
    glm::vec3 offset = glm::vec3(0);   // Local offset from center
};

struct PhysicsComponent {
    std::shared_ptr<RigidBody> rigidBody;
    std::shared_ptr<Collider> collider;
};
```

**Responsibilities:**
- Store velocity, mass, forces
- Updated by PhysicsSystem each frame
- Read by rendering for culling/bounds

#### 3. **Script Component**
Executes custom behavior via Lua or C++ callbacks.

```cpp
struct ScriptComponent {
    std::string scriptPath;           // e.g., "scripts/player_controller.lua"
    void* scriptInstance = nullptr;   // Runtime script state (Lua table)
    std::map<std::string, float> variables;  // Script variables
};
```

**Lifecycle:**
- `OnStart()` — called once when scene loads or object instantiated
- `OnUpdate(deltaTime)` — called each frame
- `OnDestroy()` — called when object removed or scene unloaded

#### 4. **Custom Components**
Extensible system for game-specific functionality.

```cpp
class IComponent {
public:
    virtual ~IComponent() = default;
    virtual void OnStart() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnDestroy() {}
};
```

---

## 3. Frame Execution Flow

### One Frame Iteration

```
┌─────────────────────────────────────────────────────────┐
│ MAIN THREAD - One Frame Loop                            │
├─────────────────────────────────────────────────────────┤
│ 1. Input & Camera                                        │
│    └─ Poll events, update camera from WASD              │
│                                                          │
│ 2. Physics Update (MAIN or PARALLEL)                     │
│    ├─ Broad phase: find potential collisions            │
│    ├─ Narrow phase: precise collision detection         │
│    ├─ Constraint solving: impulse-based resolution      │
│    ├─ Velocity integration: v += a*dt, x += v*dt        │
│    └─ Apply to transform directly                       │
│                                                          │
│ 3. Script Update                                         │
│    └─ Call OnUpdate(dt) on all ScriptComponents         │
│       │  Scripts can:                                   │
│       │  ├─ Read transforms, physics state              │
│       │  ├─ Modify transforms, force rigidbodies        │
│       │  ├─ Spawn/destroy objects                       │
│       │  └─ Play sounds, trigger events                 │
│                                                          │
│ 4. Async Cleanup (WORKER THREAD)                        │
│    └─ Enqueue TrimAll command (non-blocking ~0.01ms)    │
│       └─ Worker scans manager caches while GPU busy     │
│                                                          │
│ 5. Render Prep                                           │
│    ├─ Build render list from scene                      │
│    │  └─ For each GameObject with Renderer:             │
│    │     ├─ Get pipeline from material                  │
│    │     ├─ Get mesh vertex buffer                      │
│    │     ├─ Culling (frustum, distance, occlusion)      │
│    │     └─ Create DrawCall                             │
│    └─ Sort DrawCalls by (pipeline, mesh)                │
│                                                          │
│ 6. GPU Sync                                              │
│    ├─ vkWaitForFences (prior frame done)                │
│    ├─ ProcessPendingDestroys (safe from GPU)            │
│    └─ ~1-3ms GPU idle (physics thread can work here)    │
│                                                          │
│ 7. Record Commands                                      │
│    ├─ Bind render pass and framebuffer                  │
│    ├─ For each DrawCall:                                │
│    │  ├─ Bind pipeline and descriptors                  │
│    │  ├─ Bind vertex buffer                             │
│    │  ├─ vkCmdDynamicState (viewport, scissor)          │
│    │  ├─ Push push-constants (transform, color)         │
│    │  └─ vkCmdDraw                                      │
│    └─ End render pass, close command buffer             │
│                                                          │
│ 8. GPU Submission                                        │
│    ├─ vkQueueSubmit (GPU starts rendering)              │
│    ├─ vkQueuePresentKHR (swap buffers)                  │
│    └─ Returns immediately; GPU is busy                  │
│                                                          │
│ [LOOP BACK → Frame N+1]                                 │
│ [Meanwhile: GPU renders frame N, PhysicsThread updates] │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

**Key Timing:**
- Input + Physics + Script: **~0–5 ms** (depends on scene complexity)
- vkWaitForFences: **~1–3 ms** (GPU sync point)
- Render list builder: **~0.1–1 ms**
- Record + Submit: **~0.5–2 ms**
- Worker thread cleanup: **~0.3–0.7 ms** (overlapped with GPU, ~0 added cost)
- **Total per frame at 60 FPS:** ~16.67 ms available; budget varies by scene

---

## 4. Physics System

### Architecture

```
┌───────────────────────────────────────┐
│ PhysicsWorld (Singleton)              │
│ ├─ Global gravity                     │
│ ├─ Time scale                         │
│ └─ Broad phase partioning (grid/tree) │
└───────────────────────────────────────┘
         ↓
┌───────────────────────────────────────┐
│ Broad Phase                           │
│ └─ Spatial partitioning               │
│    └─ Gather potential collision pairs│
└───────────────────────────────────────┘
         ↓
┌───────────────────────────────────────┐
│ Narrow Phase                          │
│ ├─ Sphere vs Sphere                   │
│ ├─ Box vs Box (AABB)                  │
│ ├─ Sphere vs Box                      │
│ └─ → Contact Manifold (points, normal)│
└───────────────────────────────────────┘
         ↓
┌───────────────────────────────────────┐
│ Constraint Solver (Iterative)         │
│ ├─ Contact impulse resolution         │
│ ├─ Friction constraints               │
│ ├─ Restitution (bounce)               │
│ └─ → Apply impulses to RigidBodies    │
└───────────────────────────────────────┘
         ↓
┌───────────────────────────────────────┐
│ Velocity Integration                  │
│ ├─ v += a * dt  (gravity + forces)    │
│ ├─ x += v * dt                        │
│ └─ Apply damping                      │
└───────────────────────────────────────┘
         ↓
┌───────────────────────────────────────┐
│ Update GameObject Transforms          │
│ └─ Position, rotation from physics    │
└───────────────────────────────────────┘
```

### Types

- **Static** — never moves, mass = ∞; for terrain, walls, platforms
- **Dynamic** — affected by forces, gravity, collisions
- **Kinematic** — moves via script, not affected by forces; for moving platforms, NPCs

### Spatial Partitioning (Broad Phase)

Two options:

1. **Grid-based** — Fixed cell size; good for dense scenes
   - Pros: Fast lookup, simple
   - Cons: Overhead for sparse worlds

2. **Octree** — Recursive subdivision; good for sparse worlds
   - Pros: Adaptive, efficient for large worlds
   - Cons: More complex, rebuild cost

**Current plan:** Grid for MVP; swap to octree if needed.

### Solver Strategy

**Iterative impulse-based resolution** (similar to libraries like Rapier, Bullet):
1. For each contact constraint: compute impulse needed to separate bodies
2. Apply impulse: `v_a -= impulse / mass_a`, `v_b += impulse / mass_b`
3. Repeat 4–8 iterations until stable

**Friction:** Tangential impulse proportional to contact impulse and friction coefficient.

**Restitution:** Coefficient of restitution controls bounce; 0 = no bounce, 1 = fully elastic.

---

## 5. Scripting System

### Design: Lua with C++ API

**Why Lua:**
- Lightweight, fast to compile
- Easy to embed
- Familiar for game devs
- Hot-reload possible

### Script Lifecycle

```
1. Load Script File (async via JobQueue)
2. Compile/Parse (main thread, when ready)
3. Create Script Instance (table in Lua runtime)
4. Bind GameObject API (functions for GetComponent, etc.)
5. Call OnStart() → script initialization
   └─ Script may do: load config, cache component refs, init variables
6. Each Frame
   ├─ Call OnUpdate(dt)
   │  └─ Script reads/writes GameObject state
   └─ Repeat
7. Cleanup: OnDestroy() → script teardown
   └─ Script may do: save state, release resources
```

### Script API (C++ ← → Lua)

Exposed to scripts:

```lua
-- Transform queries
local pos = transform:GetPosition()
local rot = transform:GetRotation()
transform:SetPosition(x, y, z)
transform:SetRotation(quat)

-- Component access
local rigidbody = gameobject:GetPhysics()
rigidbody:ApplyForce(fx, fy, fz)
rigidbody:SetVelocity(vx, vy, vz)

-- Scene queries
local other = scene:FindGameObjectByName("Enemy")
local all = scene:FindGameObjectsByTag("Collectable")

-- Object lifecycle
scene:Instantiate(prefabPath, position)
scene:Destroy(gameobject)

-- Timers and events
timer:Wait(seconds)
event:Emit("PlayerDied", {score = 100})
event:On("Collision", onCollisionFunction)
```

### Example Script

```lua
-- scripts/player_controller.lua
local speed = 5.0
local jumpForce = 10.0
local grounded = false

function OnStart(gameobject)
    -- Cache component references
    transform = gameobject:GetTransform()
    physics = gameobject:GetPhysics()
end

function OnUpdate(dt, gameobject)
    -- Read input (from input manager)
    local input = engine:GetInput()
    local moveX = 0
    if input:IsKeyPressed("W") then moveX = moveX + 1 end
    if input:IsKeyPressed("S") then moveX = moveX - 1 end
    
    -- Apply movement
    if moveX ~= 0 then
        local vel = physics:GetVelocity()
        vel.x = moveX * speed
        physics:SetVelocity(vel)
    end
    
    -- Jump
    if input:IsKeyPressed("Space") and grounded then
        physics:ApplyForce(0, jumpForce, 0)
        grounded = false
    end
end

function OnCollision(other)
    if other:GetName() == "Ground" then
        grounded = true
    end
end

function OnDestroy()
    -- Cleanup
end
```

---

## 6. Data Layout (Memory Organization)

### Structure of Arrays (SoA) Pattern

**Goal:** Cache efficiency for physics and scripting loops.

Instead of:
```cpp
struct GameObject { Transform t; Renderer r; Physics p; };
std::vector<GameObject> objects;  // Bad cache coherency
```

Use:
```cpp
std::vector<Transform> transforms;
std::vector<Renderer> renderers;
std::vector<RigidBody> rigidBodies;
std::vector<ScriptComponent> scripts;

// GameObject is just metadata + indices
struct GameObject {
    uint32_t id;
    bool bHasRenderer, bHasPhysics, bHasScript;
    uint32_t rendererIdx, physicsIdx, scriptIdx;
};
std::vector<GameObject> gameObjects;
```

**Physical Memory Layout:**

```
Transform Pool (iterate for physics):
┌────┬────┬────┬────┬────┐
│ T0 │ T1 │ T2 │ T3 │ T4 │  ← CPU cache-friendly iteration
└────┴────┴────┴────┴────┘

RigidBody Pool (iterate for physics):
┌────┬────┬────┬────┬────┐
│ RB0│ RB1│ RB2│ RB3│ RB4│  ← Same objects, locality preserved
└────┴────┴────┴────┴────┘

Renderer Pool (iterate for culling/rendering):
┌────┬────┬────┬────┬────┐
│ R0 │ R1 │ R2 │ R3 │ R4 │  ← Only rendered objects
└────┴────┴────┴────┴────┘

GameObject indices:
┌──────┬──────┬──────┬──────┬──────┐
│ GO#0 │ GO#1 │ GO#2 │ GO#3 │ GO#4 │  ← Maps to pools above
└──────┴──────┴──────┴──────┴──────┘
  rIdx=0 rIdx=1 rIdx=-- rIdx=2 rIdx=3
  pIdx=0 pIdx=0 pIdx=-- pIdx=-- pIdx=1
```

**Advantage:** When looping `for (auto& t : transforms) { ... }`, we iterate contiguously in memory → CPU cache hits, no cache misses.

### Component Manager

```cpp
class ComponentManager {
    std::unordered_map<uint32_t, uint32_t> goIdToTransformIdx;
    std::unordered_map<uint32_t, uint32_t> goIdToRendererIdx;
    std::unordered_map<uint32_t, uint32_t> goIdToPhysicsIdx;
    // ...
    
    Transform* GetTransform(uint32_t gameObjectId) {
        auto it = goIdToTransformIdx.find(gameObjectId);
        if (it != goIdToTransformIdx.end())
            return &transforms[it->second];
        return nullptr;
    }
};
```

---

## 7. Threading Architecture

### Threads

| Thread | Role | Startup | Synchronization |
|--------|------|---------|------------------|
| **Main** | Input, Physics, Script, Rendering | On app start | Orchestrates frame |
| **ResourceManager** | Async resource cleanup (TrimUnused, ProcessDestroys) | OnInitVulkan | Enqueue/dequeue commands |
| **JobQueue** | File I/O, shader compilation, mesh loading, texture loading | On app start | Callbacks to main thread |
| **Physics (optional)** | Heavy physics for large scenes; parallel to main thread | If enabled | Barrier before render |
| **GPU** | Rendering; fence synchronization with main thread | N/A (implicit) | vkWaitForFences, vkQueueSubmit |

### Synchronization Points

#### 1. **Frame Boundary (vkWaitForFences)**
Main thread waits for GPU to finish prior frame.
- Typical duration: 1–3 ms
- During this time: Physics/script can still run, resource cleanup thread executes

#### 2. **Physics Barrier (if parallel physics)**
Main thread waits for physics thread to finish before rendering.
- Ensures transforms are up-to-date

#### 3. **Resource Enqueue** (lock-free)
Main thread enqueues TrimAll command to resource manager thread.
- Duration: ~0.01 ms
- No blocking; resource thread schedules work

#### 4. **Command Recording**
Single-threaded; no race conditions.

#### 5. **GPU Submission**
Returns immediately; GPU processes commands asynchronously.

### Parallelism Opportunities

**Frame N:** Main thread records & submits; GPU renders frame N−1; Resource thread cleans up; Physics thread (optional) works on frame N+1 logic.

```
Timeline:
┌─ Frame N−1 ─┬─ Frame N ─┬─ Frame N+1 ─┐
│ GPU busy    │ GPU busy  │ GPU busy    │
│ (render)    │ (render)  │ (render)    │
└─────────────┴───────────┴─────────────┘
              ├─ Main thread: Physics, Script, Render Recording ─┤
              ├─ Resource thread: Trim, Destroy (async) ─┤
              ├─ Physics thread (opt): Heavy calculations ─┤
              ├─ JobQueue: Loading ─┤
```

---

## 8. Integration: How It All Fits

### Initialization Order

```
1. Window creation (SDL3)
2. Vulkan instance, device, swapchain, render pass
3. Create managers: MeshManager, TextureManager, MaterialManager, PipelineManager, ShaderManager
4. Create PhysicsWorld
5. Create ScriptManager
6. Start ResourceManagerThread
7. Start JobQueue
8. Load scene (async or sync)
9. Enter MainLoop
```

### Scene Load Sequence

```
1. File load (JobQueue)
2. Parse JSON (main thread, when ready)
   ├─ For each material: RegisterMaterial
   ├─ For each mesh: RequestLoadMesh
   ├─ For each texture: RequestLoadTexture
3. Main thread waits for assets ready (GetMesh, GetTexture)
4. For each object in JSON:
   ├─ Create GameObject
   ├─ Add Renderer component
   ├─ Add Physics component (if physics in JSON)
   ├─ Add Script component (if script in JSON)
   ├─ Call ScriptComponent::OnStart()
   └─ Add to scene
5. Scene fully loaded; frame loop begins
```

### Cleanup (Scene Unload)

```
1. Scene::Clear() → deletes all GameObjects
2. All component refs drop (use_count decreases)
3. Next frame: ResourceManagerThread::TrimUnused
   ├─ MaterialManager removes unused
   ├─ MeshManager removes unused
   ├─ TextureManager removes unused
   ├─ PipelineManager removes unused
4. ProcessPendingDestroys (after vkWaitForFences)
   └─ Destructors release Vulkan resources
5. Scene memory cleaned; ready for next load
```

---

## 9. Module Interactions

### Physics → Script

After physics updates transforms, scripts read them:

```cpp
// Physics updates (main or parallel)
physics.Update(dt);  // Updates transforms

// Script reads
OnUpdate(dt) {
    local pos = transform:GetPosition()  // Gets updated transform
    if pos.y < -10 then
        destroy_object()  // Fall death
    end
}
```

### Script → Physics

Scripts can apply forces:

```lua
function OnUpdate(dt)
    if input:IsKeyPressed("Space") then
        rigidbody:ApplyForce(0, jumpForce, 0)
    end
end
```

### Physics → Rendering

RenderListBuilder reads transforms set by physics:

```cpp
for (auto& go : scene.gameObjects) {
    if (!go.bHasRenderer) continue;
    
    auto& renderer = renderers[go.rendererIdx];
    auto& transform = transforms[go.transformIdx];  // ← Set by physics
    
    // Use transform to build DrawCall (MVP for push constant)
    // ...
}
```

### Rendering → Physics

No direct interaction; fully decoupled.

---

## 10. Performance Targets

| Operation | Target | Typical |
|-----------|--------|----------|
| Physics (100 objects) | < 2 ms | 0.5–1.5 ms |
| Script OnUpdate (100 scripts) | < 1 ms | 0.2–0.8 ms |
| Render list building (1000 objects, 100 visible) | < 1 ms | 0.1–0.5 ms |
| GPU command recording (100 draw calls) | < 2 ms | 0.5–1 ms |
| Resource cleanup (async) | < 1 ms | 0.3–0.7 ms |
| **Total main thread (no physics parallel)** | < 16.67 ms (60fps) | 3–8 ms (60fps typical) |

**Key insight:** Main thread typically has 8–12 ms available for game logic; async cleanup adds zero latency.

---

## 11. What's Missing: Comprehensive Gap Analysis

### Current State (Phase 2.5 Complete)

**What Exists ✅:**
- Hardcoded directional light in fragment shaders
  - Direction: `(0.5, 1.0, 0.3)`, ambient: `0.3`
  - Simple N·L diffuse applied to all objects
- Emissive storage in Object struct (`emissive[4]`)
  - Loaded from glTF `emissiveFactor`
  - **BUT: Never used in shader** — emissive materials don't glow
- Per-vertex normals (computed by mesh loaders)
  - **BUT: No model matrix transform** — normals stay in local space

**Critical Bugs ❌:**

1. **Broken Normal Transforms**
   - Vertex shader passes local-space normals without model matrix
   - Rotated objects have incorrect lighting (normals don't rotate)
   - **Fix:** Add model matrix to push constant/UBO, transform normals

2. **Emissive Data Orphaned**
   - `Object.emissive[4]` populated but shader never reads it
   - **Fix:** Pass to fragment shader, add `finalColor += emissive * strength`

3. **Material Properties Incomplete**
   - Loaded: baseColor ✅, emissive ✅
   - **NOT Loaded:** metallic ❌, roughness ❌, normalScale ❌, occlusion ❌
   - **Fix:** Load from glTF, store in Object, expose via UBO

---

### Missing Systems (Phase 3 Requirements)

| System | Status | Blocker? | Effort |
|--------|--------|----------|--------|
| **GameObject/Component Architecture** | ❌ Not implemented | YES | 2-3 days |
| **Light Component System** | ❌ Not implemented | YES | 1-2 days |
| **Light Manager** | ❌ Not implemented | YES | 1-2 days |
| **Material Property UBO** | ❌ Not implemented | YES | 2-3 days |
| **Multi-light Forward Shader** | ❌ Not implemented | YES | 1-2 days |
| **Descriptor Bindings (lights)** | ⚠️ Partial | YES | 1-2 days |
| **Light Culling** | ❌ Not implemented | NO | 1-2 days |
| **Light Visualization** | ❌ Not implemented | NO | 4-6 hours |

---

### Tier 1: Critical Fixes (Before Phase 3)

#### 1.1 Fix Normal Transform Bug
```cpp
// Current (BROKEN):
// vert.vert: outNormal = inNormal;  // No transform!

// Required:
// Add to push constant or UBO:
struct PushConstant {
    mat4 mvp;           // Model-View-Projection
    mat4 modelMatrix;   // NEW: for normal transform
    vec4 color;
};

// Vertex shader:
outNormal = mat3(transpose(inverse(pc.modelMatrix))) * inNormal;
outWorldPos = (pc.modelMatrix * vec4(inPosition, 1.0)).xyz;
```

**Files:** `shaders/source/vert.vert`, `src/scene/object.h`, `src/render/render_list_builder.cpp`

#### 1.2 Use Emissive in Shader
```glsl
// Fragment shader addition:
layout(push_constant) uniform Push {
    // ... existing ...
    vec4 emissive;  // RGB + strength
} pc;

void main() {
    // ... existing lighting ...
    vec3 finalColor = baseColor * lighting;
    finalColor += pc.emissive.rgb * pc.emissive.a;  // Self-illumination
    outColor = vec4(finalColor, alpha);
}
```

**Files:** `shaders/source/frag.frag`, `src/scene/object.h`

#### 1.3 Load Material Properties from glTF
```cpp
// In SceneManager::LoadGltfMeshesFromFile:
// ADD after loading baseColorFactor:
obj.metallic = static_cast<float>(gltfMat.pbrMetallicRoughness.metallicFactor);
obj.roughness = static_cast<float>(gltfMat.pbrMetallicRoughness.roughnessFactor);
obj.emissiveStrength = 1.0f;  // Default; increase for brighter glow
```

**Files:** `src/scene/object.h` (add fields), `src/managers/scene_manager.cpp`

---

### Tier 2: GameObject/Component Foundation

#### 2.1 GameObject System
```cpp
// src/core/gameobject.h
struct GameObject {
    uint32_t id;                          // Unique identifier
    std::string name;                     // Human-readable
    bool bActive = true;                  // Enable/disable
    
    // Transform (always present)
    glm::vec3 position = glm::vec3(0);
    glm::quat rotation = glm::quat(1,0,0,0);
    glm::vec3 scale = glm::vec3(1);
    
    // Component presence flags
    bool bHasRenderer = false;
    bool bHasPhysics = false;
    bool bHasScript = false;
    bool bHasLight = false;
    
    // Indices into component pools (UINT32_MAX = not present)
    uint32_t rendererIndex = UINT32_MAX;
    uint32_t physicsIndex = UINT32_MAX;
    uint32_t scriptIndex = UINT32_MAX;
    uint32_t lightIndex = UINT32_MAX;
};
```

#### 2.2 Component Base
```cpp
// src/core/component.h
class IComponent {
public:
    virtual ~IComponent() = default;
    virtual void OnStart() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnDestroy() {}
};
```

#### 2.3 Light Component
```cpp
// src/lighting/light_component.h
enum class LightType {
    Directional,   // Sun-like, infinite distance
    Point,         // Omnidirectional, falloff with distance
    Spot,          // Cone-shaped, directional + falloff
    Area,          // Rectangular emitter (Phase 4+)
};

struct LightComponent {
    LightType type = LightType::Point;
    
    glm::vec3 color = glm::vec3(1);       // RGB color
    float intensity = 1.0f;                // Brightness (0 to infinity)
    
    float range = 10.0f;                   // Max distance for point/spot
    float falloffExponent = 2.0f;          // Attenuation: 1/distance^exp
    
    // For spotlights:
    float innerConeAngle = 0.0f;           // Full brightness (radians)
    float outerConeAngle = M_PI / 4.0f;    // Fade to zero (radians)
    glm::vec3 direction = glm::vec3(0,-1,0); // Updated from transform
    
    bool bCastShadow = false;              // Phase 4+
    bool bActive = true;
};
```

#### 2.4 Scene with Component Pools
```cpp
// src/core/scene.h
struct Scene {
    std::string name;
    bool bActive = true;
    
    // GameObject array (lightweight metadata + indices)
    std::vector<GameObject> gameObjects;
    
    // Component pools (Structure of Arrays for cache efficiency)
    std::vector<Transform> transforms;
    std::vector<RendererComponent> renderers;
    std::vector<PhysicsComponent> physics;
    std::vector<LightComponent> lights;
    std::vector<ScriptComponent> scripts;
    
    // Component manager for fast lookup
    std::unordered_map<uint32_t, uint32_t> goIdToTransformIdx;
    std::unordered_map<uint32_t, uint32_t> goIdToRendererIdx;
    std::unordered_map<uint32_t, uint32_t> goIdToPhysicsIdx;
    std::unordered_map<uint32_t, uint32_t> goIdToLightIdx;
    std::unordered_map<uint32_t, uint32_t> goIdToScriptIdx;
    
    void Clear();
    GameObject* FindGameObjectByID(uint32_t id);
    GameObject* FindGameObjectByName(const std::string& name);
};
```

---

### Tier 3: Lighting Infrastructure

#### 3.1 Light Manager
```cpp
// src/lighting/light_manager.h
class LightManager {
public:
    void SetScene(Scene* scene);
    
    // Update light transforms from GameObjects each frame
    void UpdateLightTransforms();
    
    // Culling: get lights affecting a world position
    std::vector<uint32_t> GetLightsInRadius(glm::vec3 worldPos, float radius) const;
    
    // GPU upload
    void UploadLightDataToGPU(VkDevice device, VkCommandBuffer cmd);
    
    // Accessors
    const std::vector<LightGPUData>& GetGPULightData() const;
    uint32_t GetActiveLightCount() const;
    
    VkBuffer GetLightBuffer() const { return m_lightBuffer; }
    
private:
    Scene* m_pScene = nullptr;
    
    // CPU-side light data
    std::vector<uint32_t> m_activeLightIndices;
    
    // GPU-side data
    struct LightGPUData {
        glm::vec4 position;    // xyz = pos, w = range
        glm::vec4 color;       // rgb = color, a = intensity
        glm::vec4 direction;   // xyz = dir (for spot), w = type
        glm::vec4 params;      // x = innerCone, y = outerCone, z = falloff, w = unused
    };
    std::vector<LightGPUData> m_gpuLightData;
    
    VkBuffer m_lightBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_lightBufferMemory = VK_NULL_HANDLE;
    size_t m_lightBufferSize = 0;
};
```

#### 3.2 Material Property UBO
```cpp
// src/vulkan/material_property_buffer.h
struct MaterialProperties {
    glm::vec3 baseColor;           // RGB
    float metallic;                // 0 to 1
    
    glm::vec3 emissive;            // RGB
    float roughness;               // 0 to 1
    
    float emissiveStrength;        // 0 to 10+ (brightness multiplier)
    float normalScale;             // 0 to 1 (normal map strength)
    float occlusionStrength;       // 0 to 1 (AO map strength)
    float _pad;                    // Alignment
};

// Per-object instance data
struct ObjectInstanceData {
    glm::mat4 modelMatrix;         // Transform
    glm::mat4 normalMatrix;        // transpose(inverse(model)) for normals
    MaterialProperties material;
};

class MaterialPropertyBuffer {
public:
    void Create(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t maxInstances);
    void Destroy(VkDevice device);
    
    void UpdateInstance(uint32_t index, const ObjectInstanceData& data);
    void Upload(VkDevice device, VkCommandBuffer cmd);
    
    VkBuffer GetBuffer() const { return m_buffer; }
    VkDescriptorBufferInfo GetDescriptorInfo(uint32_t instanceIndex) const;
    
private:
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    void* m_mapped = nullptr;
    uint32_t m_maxInstances = 0;
    size_t m_instanceSize = 0;
};
```

#### 3.3 Multi-Light Fragment Shader
```glsl
// shaders/source/frag.frag
#version 450

#define MAX_LIGHTS 256

layout(push_constant) uniform Push {
    mat4 mvp;
    vec4 color;
} pc;

layout(set = 0, binding = 0) uniform sampler2D uTex;

layout(set = 0, binding = 1) uniform MaterialBlock {
    vec3 baseColor;
    float metallic;
    vec3 emissive;
    float roughness;
    float emissiveStrength;
    float normalScale;
    float occlusionStrength;
    float _pad;
} material;

struct Light {
    vec4 position;    // xyz = world pos, w = range
    vec4 color;       // rgb = color, a = intensity
    vec4 direction;   // xyz = dir (spot), w = type (0=dir, 1=point, 2=spot)
    vec4 params;      // x = innerCone, y = outerCone, z = falloff, w = unused
};

layout(set = 0, binding = 2) uniform LightBlock {
    uint lightCount;
    uint _pad1, _pad2, _pad3;
    Light lights[MAX_LIGHTS];
} lightData;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(uTex, inUV);
    vec3 normal = normalize(inNormal);
    
    // Start with emissive (self-illumination)
    vec3 lighting = material.emissive * material.emissiveStrength;
    
    // Add global ambient
    lighting += vec3(0.2);
    
    // Accumulate light contributions
    for (uint i = 0; i < lightData.lightCount && i < MAX_LIGHTS; ++i) {
        Light light = lightData.lights[i];
        
        vec3 toLight = light.position.xyz - inWorldPos;
        float distance = length(toLight);
        vec3 lightDir = normalize(toLight);
        
        // Attenuation: 1 / (1 + (distance / range)^falloff)
        float attenuation = 1.0 / (1.0 + pow(distance / light.position.w, light.params.z));
        
        // Diffuse: N·L
        float ndotl = max(dot(normal, lightDir), 0.0);
        
        // Spotlight cone
        float spotFactor = 1.0;
        if (light.direction.w > 1.5) {  // Type == Spot
            float cosTheta = dot(-lightDir, normalize(light.direction.xyz));
            float innerCos = cos(light.params.x);
            float outerCos = cos(light.params.y);
            spotFactor = smoothstep(outerCos, innerCos, cosTheta);
        }
        
        // Accumulate contribution
        vec3 contribution = light.color.rgb * light.color.a * ndotl * attenuation * spotFactor;
        lighting += contribution;
    }
    
    vec3 finalColor = texColor.rgb * material.baseColor * lighting;
    outColor = vec4(finalColor, texColor.a * pc.color.a);
}
```

---

### Tier 4: Rendering Integration

#### 4.1 Updated RenderListBuilder
```cpp
// In RenderListBuilder::Build:
for (const auto& go : scene.gameObjects) {
    if (!go.bActive || !go.bHasRenderer) continue;
    
    const RendererComponent& renderer = scene.renderers[go.rendererIndex];
    const Transform& transform = scene.transforms[go.transformIdx];
    
    DrawCall call;
    call.pipeline = renderer.material->GetPipeline();
    call.mesh = renderer.mesh;
    call.texture = renderer.texture;
    
    // NEW: Build model matrix from transform
    glm::mat4 model = glm::translate(glm::mat4(1), transform.position);
    model *= glm::mat4_cast(transform.rotation);
    model *= glm::scale(glm::mat4(1), transform.scale);
    
    call.modelMatrix = model;
    call.mvp = viewProj * model;
    
    // NEW: Cull lights to this object
    call.affectingLights = lightManager.GetLightsInRadius(
        transform.position, 
        renderer.mesh->GetBoundingRadius() + maxLightRange
    );
    
    renderList.push_back(call);
}
```

---

## 12. File Organization & Structure

### Current Structure (Phase 2.5)
```
src/
├── app/
│   └── vulkan_app.h / .cpp ................. Main application loop
├── camera/
│   ├── camera.h / .cpp ..................... Camera math (view, projection)
│   └── camera_controller.h ................. Input handling (WASD)
├── config/
│   ├── config_loader.h / .cpp .............. JSON config parsing
│   └── vulkan_config.h / .cpp .............. Vulkan settings
├── loaders/
│   ├── gltf_loader.h / .cpp ................ glTF model loading
│   ├── gltf_mesh_utils.h / .cpp ............ Mesh extraction utilities
│   └── procedural_mesh_factory.h / .cpp .... Geometric primitives
├── managers/
│   ├── descriptor_pool_manager.h / .cpp .... Vulkan descriptor pools
│   ├── descriptor_set_layout_manager.h / .cpp Layout caching
│   ├── material_manager.h / .cpp ........... Material registry
│   ├── mesh_manager.h / .cpp ............... Mesh caching + async load
│   ├── pipeline_manager.h / .cpp ........... Pipeline caching
│   ├── resource_cleanup_manager.h / .cpp ... Cleanup orchestrator
│   ├── scene_manager.h / .cpp .............. Scene loading/unloading
│   ├── texture_manager.h / .cpp ............ Texture caching + async load
│   └── managers.h .......................... Aggregate header
├── render/
│   ├── render_list_builder.h / .cpp ........ DrawCall generation
│   └── (future: deferred/, forward/, etc.)
├── scene/
│   ├── object.h ............................ Object struct (current)
│   └── scene.h ............................. Scene container (current)
├── thread/
│   ├── job_queue.h / .cpp .................. Async file/shader loading
│   └── resource_manager_thread.h / .cpp .... Async resource cleanup
├── vulkan/
│   ├── vulkan_*.h / .cpp ................... Vulkan abstractions
│   └── vulkan_utils.h / .cpp ............... Helpers, logging
├── window/
│   └── window.h / .cpp ..................... SDL3 window management
└── main.cpp ................................ Entry point
```

### Proposed Structure (Phase 3+)
```
src/
├── app/
│   └── vulkan_app.h / .cpp
│
├── core/ ................................. NEW FOLDER
│   ├── gameobject.h / .cpp ............... GameObject system
│   ├── component.h ....................... IComponent base class
│   ├── scene.h / .cpp .................... Scene + component pools
│   ├── transform.h / .cpp ................ Transform component
│   └── README.md ......................... Core system docs
│
├── lighting/ ............................. NEW FOLDER
│   ├── light_component.h ................. Light data structures
│   ├── light_manager.h / .cpp ............ Light tracking + culling
│   ├── light_utils.h / .cpp .............. Attenuation, cone math
│   ├── shadow_map.h / .cpp (Phase 5) ..... Shadow rendering
│   └── README.md ......................... Lighting system docs
│
├── physics/ .............................. NEW FOLDER (Phase 3.5+)
│   ├── physics_world.h / .cpp ............ Simulation loop
│   ├── rigidbody.h / .cpp ................ RigidBody component
│   ├── collider.h / .cpp ................. Collider shapes
│   ├── collision_detection.h / .cpp ...... Broad/narrow phase
│   ├── constraint_solver.h / .cpp ........ Impulse solver
│   └── README.md ......................... Physics system docs
│
├── scripting/ ............................ NEW FOLDER (Phase 4+)
│   ├── script_manager.h / .cpp ........... Lua VM management
│   ├── script_component.h / .cpp ......... ScriptComponent
│   ├── script_api.h / .cpp ............... C++ ↔ Lua bindings
│   ├── lua_binding.h / .cpp .............. Binding utilities
│   └── README.md ......................... Scripting system docs
│
├── render/
│   ├── render_list_builder.h / .cpp ...... DrawCall generation
│   ├── draw_call.h ....................... DrawCall struct
│   ├── forward/ .......................... NEW SUBFOLDER (Phase 3)
│   │   ├── forward_renderer.h / .cpp ..... Multi-light forward pass
│   │   └── light_culling.h / .cpp ........ Per-object light lists
│   ├── deferred/ ......................... NEW SUBFOLDER (Phase 5)
│   │   ├── gbuffer.h / .cpp .............. G-Buffer management
│   │   ├── deferred_renderer.h / .cpp .... Deferred lighting pass
│   │   └── light_clustering.h / .cpp ..... Screen-space clustering
│   └── README.md ......................... Rendering architecture
│
├── vulkan/
│   ├── vulkan_*.h / .cpp ................. Existing abstractions
│   ├── material_property_buffer.h / .cpp . NEW: Material UBO manager
│   ├── light_data_buffer.h / .cpp ........ NEW: Light SSBO/UBO
│   └── vulkan_utils.h / .cpp
│
├── managers/ ............................. Keep existing
│   └── ... (all current managers)
│
├── loaders/ .............................. Keep existing
│   └── ... (gltf, procedural, etc.)
│
├── camera/ ............................... Keep existing
├── config/ ............................... Keep existing
├── thread/ ............................... Keep existing
├── window/ ............................... Keep existing
└── main.cpp

shaders/source/
├── vert.vert ............................. Standard vertex shader
├── frag.frag ............................. Multi-light fragment
├── frag_untextured.frag .................. Untextured variant
├── unlit.frag ............................ NEW: Debug/emissive only
├── shadow.vert / .frag (Phase 5) ......... Shadow map generation
├── deferred/ (Phase 5) ................... NEW SUBFOLDER
│   ├── gbuffer.vert / .frag .............. G-Buffer pass
│   ├── lighting.comp ..................... Light clustering compute
│   └── compose.vert / .frag .............. Final composition
└── README.md ............................. Shader documentation

docs/
├── architecture.md ....................... Module overview (keep)
├── engine-architecture.md ................ THIS FILE (updated)
├── plan-loading-and-managers.md .......... Resource management (keep)
├── lighting-system-roadmap.md ............ NEW: Lighting details
├── ROADMAP.md ............................ Development phases (keep)
├── architecture-text-tree.md ............. Text tree view (keep)
└── api/ .................................. NEW FOLDER
    ├── gameobject-api.md ................. GameObject/Component API
    ├── lighting-api.md ................... LightManager API
    ├── physics-api.md .................... Physics API
    └── scripting-api.md .................. Lua scripting API
```

### Component Folder Guidelines

**core/** — Fundamental engine systems
- GameObject/Component architecture
- Scene management with component pools
- Transform system
- Entity lifecycle management

**lighting/** — All lighting-related code
- Light components and data structures
- Light manager (registration, culling, GPU upload)
- Shadow map generation (Phase 5)
- Light probe baking (Phase 5)

**physics/** — Physics simulation
- Physics world singleton
- RigidBody and Collider components
- Collision detection (broad/narrow phase)
- Constraint solver (impulse-based)

**scripting/** — Lua scripting integration
- Lua VM lifecycle
- Script components
- C++ ↔ Lua API bindings
- Script hot-reload

**render/** — Rendering pipelines
- `forward/` — Multi-light forward rendering (Phase 3)
- `deferred/` — Deferred rendering + clustering (Phase 5)
- RenderListBuilder (culling, sorting, draw call generation)

**vulkan/** — Low-level Vulkan wrappers
- Device, swapchain, pipelines, buffers
- Material property UBO manager
- Light data SSBO/UBO manager
- Utility functions

**managers/** — Resource management
- Material, Mesh, Texture, Pipeline, Shader managers
- Descriptor pool/layout managers
- Scene manager (loading/unloading)
- Resource cleanup orchestrator

---

## 13. Implementation Roadmap

### Week 1: Critical Fixes + GameObject Foundation
**Goal:** Fix broken systems, establish component architecture

1. **Day 1-2: Critical Shader Fixes**
   - Fix normal world-space transform (add model matrix)
   - Use emissive in fragment shader
   - Load metallic/roughness from glTF
   - **Deliverable:** Existing scenes render correctly with proper normals

2. **Day 3-5: GameObject/Component System**
   - Create `src/core/` folder structure
   - Implement GameObject struct
   - Implement IComponent base class
   - Create Scene with component pools (SoA layout)
   - **Deliverable:** GameObject system compiles, basic tests pass

### Week 2: Lighting Infrastructure
**Goal:** Light components, manager, GPU data structures

3. **Day 1-2: Light Component + Manager**
   - Create `src/lighting/` folder
   - Implement LightComponent (Point, Spot, Directional)
   - Implement LightManager (registration, tracking)
   - **Deliverable:** Can create lights, track in scene

4. **Day 3-4: Material Property UBO**
   - Create `src/vulkan/material_property_buffer.h/cpp`
   - Define MaterialProperties struct
   - Implement UBO creation and upload
   - **Deliverable:** Per-object material properties on GPU

5. **Day 5: Descriptor Bindings**
   - Update DescriptorSetLayoutManager for light buffer
   - Update PipelineManager for material UBO binding
   - **Deliverable:** Shaders can access light + material data

### Week 3: Rendering Integration
**Goal:** Multi-light shaders, scene loading, rendering

6. **Day 1-2: Multi-Light Shaders**
   - Update `vert.vert` — pass world position, transform normals
   - Update `frag.frag` — light loop, attenuation, spotlight support
   - Create `unlit.frag` for debug visualization
   - **Deliverable:** Shaders compile, support multiple lights

7. **Day 3-4: Scene Loading Updates**
   - Update SceneManager to load GameObjects
   - Populate component pools from JSON
   - Support light definitions in JSON
   - **Deliverable:** Can load scenes with lights

8. **Day 5: RenderListBuilder Updates**
   - Iterate renderers from component pool
   - Build model matrices from transforms
   - Upload light data each frame
   - **Deliverable:** Scenes render with dynamic lights

### Week 4: Polish + Optimization
**Goal:** Light culling, debugging tools, performance testing

9. **Day 1-2: Light Culling Architecture**
   - Implement per-object light culling
   - Build affectingLights lists in RenderListBuilder
   - Pass culled lights to shader
   - **Deliverable:** Supports 100+ lights with culling

10. **Day 3: Debug Visualization**
    - Light spheres at light positions
    - Unlit/emissive material
    - Color-coded by light color
    - **Deliverable:** Can see lights in scene preview

11. **Day 4-5: Testing + Benchmarking**
    - Test scenes: 10, 50, 100, 200 lights
    - Profile frame times
    - Optimize hot paths
    - **Deliverable:** 60fps @ 50 lights, 1000 objects

---

## 14. Future Enhancements (Phase 4-5)

### Phase 4: Advanced Lighting
- **Normal Mapping** — Per-pixel detail (2-3 days)
- **Specular Highlights** — Blinn-Phong or PBR (1-2 days)
- **Emissive as Light Sources** — Spawn invisible point lights (1 day)
- **Animation & Skinning** — glTF skeleton support (1-2 weeks)

### Phase 5: Optimization & Advanced Rendering
- **Deferred Rendering** — G-Buffer + light clustering (2 weeks)
- **Dynamic Shadows** — Shadow maps per light (2-3 weeks)
- **Light Probes** — Baked lighting for static objects (1-2 weeks)
- **Instancing** — GPU-driven rendering (1 week)
- **Indirect Buffers** — Reduce CPU overhead (1 week)

### Additional Systems
- **Particle System** — GPU particle simulation
- **Audio System** — 3D positional audio
- **Networking** — Client-server architecture
- **Save/Load** — Scene serialization
- **Editor** — ImGui-based scene editor

---

## 12. Folder Structure (Proposed)

```
src/
├── core/
│   ├── gameobject.h / .cpp
│   ├── component.h
│   ├── scene.h / .cpp
│   ├── scene_manager.h / .cpp
│   └── script_manager.h / .cpp
├── physics/
│   ├── physics_world.h / .cpp
│   ├── rigidbody.h / .cpp
│   ├── collider.h / .cpp
│   └── collision_solver.h / .cpp
├── scripting/
│   ├── lua_runtime.h / .cpp
│   ├── script_api.h / .cpp
│   └── script_component.h / .cpp
├── managers/ (existing)
├── vulkan/ (existing)
├── app/
│   └── vulkan_app.h / .cpp (uses all subsystems)
└── ...
```

---

## See Also

- [architecture.md](architecture.md) — Module overview and Vulkan stack
- [plan-loading-and-managers.md](plan-loading-and-managers.md) — Resource managers and async cleanup
- [ROADMAP.md](ROADMAP.md) — Development phases and timeline
