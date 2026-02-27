/*
 * GameObject — Container for components in the entity-component system.
 * GameObjects are lightweight metadata; functionality comes from components.
 * Components are stored in Structure of Arrays (SoA) pools in Scene for cache efficiency.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

/** Invalid component index sentinel. */
constexpr uint32_t INVALID_COMPONENT_INDEX = UINT32_MAX;

/**
 * GameObject — Lightweight entity container.
 * Stores indices into component pools rather than component data directly.
 * This enables cache-friendly iteration over components of the same type.
 * 
 * Hierarchy: Objects can have a parent and children.
 * Transform's parentId stores the actual parent relationship.
 * Children vector is cached for efficient UI traversal.
 */
struct GameObject {
    /** Unique identifier for this GameObject. */
    uint32_t id = 0;

    /** Human-readable name (optional). */
    std::string name;

    /** Active flag. Inactive GameObjects skip update and render. */
    bool bActive = true;

    /** Transform index (always valid; every GameObject has a transform). */
    uint32_t transformIndex = INVALID_COMPONENT_INDEX;

    /** Renderer component index (INVALID_COMPONENT_INDEX if none). */
    uint32_t rendererIndex = INVALID_COMPONENT_INDEX;

    /** Light component index (INVALID_COMPONENT_INDEX if none). */
    uint32_t lightIndex = INVALID_COMPONENT_INDEX;

    /** Camera component index (INVALID_COMPONENT_INDEX if none). */
    uint32_t cameraIndex = INVALID_COMPONENT_INDEX;

    /** Physics component index (INVALID_COMPONENT_INDEX if none). Future. */
    uint32_t physicsIndex = INVALID_COMPONENT_INDEX;

    /** Script component index (INVALID_COMPONENT_INDEX if none). Future. */
    uint32_t scriptIndex = INVALID_COMPONENT_INDEX;

    /** Cached list of child GameObject IDs (for UI traversal). */
    std::vector<uint32_t> children;

    /** Check if this GameObject has a renderer component. */
    bool HasRenderer() const { return rendererIndex != INVALID_COMPONENT_INDEX; }

    /** Check if this GameObject has a light component. */
    bool HasLight() const { return lightIndex != INVALID_COMPONENT_INDEX; }

    /** Check if this GameObject has a camera component. */
    bool HasCamera() const { return cameraIndex != INVALID_COMPONENT_INDEX; }

    /** Check if this GameObject has a physics component. Future. */
    bool HasPhysics() const { return physicsIndex != INVALID_COMPONENT_INDEX; }

    /** Check if this GameObject has a script component. Future. */
    bool HasScript() const { return scriptIndex != INVALID_COMPONENT_INDEX; }

    /** Check if this GameObject has any children. */
    bool HasChildren() const { return !children.empty(); }
};

