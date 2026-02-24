/**
 * Subsystem — Base class for engine subsystems.
 *
 * Provides a consistent lifecycle interface for all major engine components.
 * Subsystems are initialized in dependency order and shutdown in reverse order.
 *
 * Lifecycle:
 *   1. Create() - Allocate resources, register with other systems
 *   2. Update() - Called each frame (optional for some subsystems)
 *   3. Shutdown() - Release all resources in reverse order
 *
 * Phase 4.4: App Separation
 */

#pragma once

#include <cstdint>
#include <string>

/**
 * SubsystemPriority — Controls initialization and update order.
 *
 * Lower values initialize first, update first, and shutdown last.
 */
enum class SubsystemPriority : int32_t {
    Core       = 0,     // Window, Input, Config
    Resources  = 100,   // Asset loading, caching
    Scene      = 200,   // ECS, scene management
    Render     = 300,   // Renderer, passes
    Editor     = 400,   // Debug tools, editor UI (Debug only)
    Runtime    = 500    // Game-specific systems
};

/**
 * Subsystem — Abstract base class for engine subsystems.
 *
 * Each subsystem manages a coherent set of resources and functionality.
 * Examples: GraphicsSubsystem, SceneSubsystem, AssetSubsystem, EditorSubsystem
 */
class Subsystem {
public:
    Subsystem() = default;
    virtual ~Subsystem() = default;

    // Non-copyable, non-movable
    Subsystem(const Subsystem&) = delete;
    Subsystem& operator=(const Subsystem&) = delete;
    Subsystem(Subsystem&&) = delete;
    Subsystem& operator=(Subsystem&&) = delete;

    /**
     * Initialize the subsystem.
     * Called once during engine startup, after all dependencies are created.
     * @return true on success, false on fatal error (engine will abort)
     */
    virtual bool Create() = 0;

    /**
     * Update the subsystem.
     * Called once per frame. Return value indicates if subsystem is still valid.
     * @param deltaTime Time since last frame in seconds
     * @return true if subsystem is healthy, false to signal shutdown
     */
    virtual bool Update(float deltaTime) { (void)deltaTime; return true; }

    /**
     * Pre-render preparation.
     * Called after Update(), before any rendering begins.
     * Use for CPU-side preparation that must complete before GPU submission.
     */
    virtual void PreRender() {}

    /**
     * Post-render cleanup.
     * Called after all rendering and presentation is complete.
     * Use for deferred cleanup, statistics gathering, etc.
     */
    virtual void PostRender() {}

    /**
     * Shutdown the subsystem.
     * Called once during engine shutdown, in reverse initialization order.
     * Must release all resources.
     */
    virtual void Shutdown() = 0;

    /**
     * Handle window resize.
     * Called when the window/swapchain is resized.
     * @param width New width in pixels
     * @param height New height in pixels
     */
    virtual void OnResize(uint32_t width, uint32_t height) { (void)width; (void)height; }

    /**
     * Get subsystem name for logging/debugging.
     */
    virtual const char* GetName() const = 0;

    /**
     * Get initialization priority.
     * Lower values initialize first.
     */
    virtual SubsystemPriority GetPriority() const = 0;

    /**
     * Check if subsystem is initialized.
     */
    bool IsInitialized() const { return m_initialized; }

protected:
    bool m_initialized = false;
};
