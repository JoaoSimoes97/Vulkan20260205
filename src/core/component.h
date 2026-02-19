/*
 * Component — Base interface for all components in the GameObject system.
 * Components provide functionality to GameObjects through composition.
 */
#pragma once

#include <cstdint>

/**
 * Component type enumeration for fast type checking without RTTI.
 */
enum class ComponentType : uint32_t {
    Transform = 0,
    Renderer,
    Light,
    Physics,
    Script,
    Custom,
    COUNT
};

/**
 * Base component interface.
 * All components inherit from this to provide lifecycle hooks.
 */
class IComponent {
public:
    virtual ~IComponent() = default;

    /** Called once when the component is added to a GameObject. */
    virtual void OnStart() {}

    /** Called each frame. deltaTime is in seconds. */
    virtual void OnUpdate(float deltaTime) { (void)deltaTime; }

    /** Called when the component is removed or the GameObject is destroyed. */
    virtual void OnDestroy() {}

    /** Get the component type for fast type checking. */
    virtual ComponentType GetType() const = 0;

    /** Component enabled state. Disabled components skip OnUpdate. */
    bool bEnabled = true;
};

