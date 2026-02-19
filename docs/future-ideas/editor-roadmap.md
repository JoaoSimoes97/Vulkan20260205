# Editor Roadmap

Design and implementation plan for the visual editor and runtime object manipulation.

**Status:** Planned (Phase 3)

---

## Goals

Enable real-time editing of all scene objects:
- Select objects by clicking in viewport
- Move, rotate, scale with gizmos
- Edit properties in inspector panel
- Undo/redo all changes
- Play/pause the scene

---

## Implementation Order

### 1. ImGui Integration (Priority: High)

**What:** Dear ImGui overlay for editor UI.

**Tasks:**
- Add ImGui to vcpkg.json
- Initialize ImGui with Vulkan backend
- Render ImGui after scene (separate render pass or same pass)
- Basic window: FPS counter, object list

**Files to create/modify:**
- `src/editor/imgui_layer.h/cpp`
- `vulkan_app.cpp` — init and render calls

---

### 2. Selection System (Priority: High)

**What:** Click-to-select objects in viewport.

**Tasks:**
- Ray casting from mouse position through camera
- Ray-AABB/Ray-Sphere intersection tests
- Selection highlight (outline or tint)
- Multi-selection with Shift+Click

**Data structures:**
```cpp
struct SelectionState {
    std::vector<uint32_t> selectedObjects;  // GameObject indices
    bool bMultiSelect = false;
};
```

**Files to create:**
- `src/editor/selection.h/cpp`
- `src/editor/ray_cast.h/cpp`

---

### 3. Inspector Panel (Priority: High)

**What:** Edit selected object's components.

**Tasks:**
- Show Transform: position, rotation (euler), scale
- Show RendererComponent: mesh name, texture, tint
- Show LightComponent: type, color, intensity, range, angles
- Editable fields with immediate apply

**UI Layout:**
```
┌─────────────────────────────────┐
│ Inspector                       │
├─────────────────────────────────┤
│ Name: [PointLight_Red      ]    │
│                                 │
│ Transform                       │
│   Position: X[-3.0] Y[2.0] Z[0] │
│   Rotation: X[0] Y[0] Z[0]      │
│   Scale:    X[1] Y[1] Z[1]      │
│                                 │
│ Light                           │
│   Type: [Point ▼]               │
│   Color: [█████] (1.0, 0.3, 0.2)│
│   Intensity: [5.0]              │
│   Range: [8.0]                  │
└─────────────────────────────────┘
```

---

### 4. Scene Hierarchy Panel (Priority: Medium)

**What:** Tree view of all GameObjects.

**Tasks:**
- List all objects by name
- Click to select
- Drag to reparent (future: parent/child transforms)
- Right-click context menu: Delete, Duplicate

---

### 5. Gizmos (Priority: Medium)

**What:** Visual handles for transform manipulation.

**Tasks:**
- Translation gizmo: 3 arrows (X, Y, Z) + center quad
- Rotation gizmo: 3 circles around axes
- Scale gizmo: 3 cubes on axes + uniform center
- Mouse drag to manipulate
- Gizmo pipeline (LINE_LIST + TRIANGLE for handles)

**Modes:**
- W = Translate
- E = Rotate  
- R = Scale
- Q = No gizmo (select only)

**Files to create:**
- `src/editor/gizmo_renderer.h/cpp`
- `shaders/source/gizmo.vert/frag`

---

### 6. Undo/Redo System (Priority: Medium)

**What:** Track all edits for reversibility.

**Pattern:** Command pattern with history stack.

```cpp
struct ICommand {
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual std::string GetDescription() = 0;
};

class CommandHistory {
    std::vector<std::unique_ptr<ICommand>> m_history;
    int m_currentIndex = -1;
    void Execute(std::unique_ptr<ICommand> cmd);
    void Undo();
    void Redo();
};
```

**Commands:**
- `TransformCommand` — modify position/rotation/scale
- `PropertyCommand` — modify any component property
- `CreateObjectCommand` / `DeleteObjectCommand`

---

### 7. Multi-Viewport (Priority: Low)

**What:** Multiple camera views (Scene, Game, Preview).

**Tasks:**
- Viewport abstraction (camera + render target)
- Scene view: editor camera with gizmos
- Game view: runtime camera without gizmos
- Dockable ImGui windows for each viewport

---

### 8. Play/Pause/Stop (Priority: Low)

**What:** Runtime control for testing.

**Tasks:**
- Play: enable physics + scripts, hide gizmos
- Pause: freeze physics + scripts, show gizmos
- Stop: reset to saved state

**State machine:**
```
Edit ──Play→ Playing ──Pause→ Paused ──Resume→ Playing
  ↑                              │
  └──────────Stop────────────────┘
```

---

## Dependencies

| Feature | Depends On |
|---------|------------|
| Inspector Panel | ImGui Integration |
| Scene Hierarchy | ImGui Integration |
| Selection System | None (can work without ImGui) |
| Gizmos | Selection System |
| Undo/Redo | Any editing feature |
| Multi-Viewport | ImGui Integration |
| Play/Pause | Physics/Scripting (Phase 4) |

---

## Estimated Effort

| Feature | Effort | Priority |
|---------|--------|----------|
| ImGui Integration | 1-2 days | High |
| Selection System | 1-2 days | High |
| Inspector Panel | 2-3 days | High |
| Scene Hierarchy | 1 day | Medium |
| Gizmos | 3-5 days | Medium |
| Undo/Redo | 2-3 days | Medium |
| Multi-Viewport | 2-3 days | Low |
| Play/Pause | 1 day | Low |

---

## References

- [Dear ImGui](https://github.com/ocornut/imgui) — Immediate mode GUI
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) — Gizmo library for ImGui
- [Unity Editor](https://docs.unity3d.com/Manual/UsingTheEditor.html) — Reference design
