/**
 * RuntimeApp — Runtime application (Release builds).
 *
 * Phase 4.4: App Separation
 *
 * The RuntimeApp provides a minimal runtime environment:
 * - Scene loading and execution
 * - Minimal FPS overlay (optional)
 * - No editing capabilities
 * - Optimized for performance
 *
 * This is what ships to end users.
 */

#pragma once

#include "../core/engine.h"
#include "../core/subsystem.h"
#include <string>

/**
 * RuntimeApp — Minimal runtime application.
 *
 * Usage:
 *   RuntimeApp app;
 *   app.Run(levelPath);
 *
 * The runtime provides:
 * - Level loading from JSON
 * - Game loop execution
 * - Minimal debug overlay (FPS, frame time)
 */
class RuntimeApp : public Subsystem {
public:
    RuntimeApp() = default;
    ~RuntimeApp() override = default;

    // Subsystem interface
    bool Create() override;
    bool Update(float deltaTime) override;
    void PreRender() override {}
    void PostRender() override;
    void Shutdown() override;
    SubsystemPriority GetPriority() const override { return SubsystemPriority::Runtime; }

    /**
     * Run the runtime with a specific level.
     * @param levelPath Path to level JSON file
     * @return Exit code (0 = success)
     */
    int Run(const std::string& levelPath = "");

    /**
     * Load a level at runtime.
     * @param levelPath Path to level JSON file
     * @return true on success
     */
    bool LoadLevel(const std::string& levelPath);

    /**
     * Check if runtime is running.
     */
    bool IsRunning() const { return m_running; }

    /**
     * Request runtime exit.
     */
    void RequestExit() { m_exitRequested = true; }

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * Enable/disable FPS overlay.
     */
    void SetShowOverlay(bool show) { m_showOverlay = show; }
    bool GetShowOverlay() const { return m_showOverlay; }

    /**
     * Set initial level to load.
     */
    void SetInitialLevel(const std::string& levelPath) { m_initialLevel = levelPath; }

private:
    // Engine integration
    Engine* m_engine = nullptr;

    // State
    bool m_running = false;
    bool m_exitRequested = false;
    std::string m_initialLevel;
    std::string m_currentLevel;

    // Debug overlay
    bool m_showOverlay = true;

    // Internal methods
    void RenderOverlay();
};
