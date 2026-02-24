/**
 * EditorApp — Editor application (Debug builds only).
 *
 * Phase 4.4: App Separation
 *
 * The EditorApp extends the base runtime with:
 * - ImGuizmo transform gizmos
 * - Level editing UI
 * - Scene hierarchy panel
 * - Property inspector
 * - Hot-reload support
 * - Debug overlays
 *
 * Compiled only when EDITOR_BUILD=1 (Debug builds).
 */

#pragma once

#if EDITOR_BUILD

#include "../core/engine.h"
#include "../core/subsystem.h"
#include <memory>
#include <string>

// Forward declarations
class EditorLayer;
class Camera;
struct SDL_Event;

/**
 * Editor camera mode.
 */
enum class EditorCameraMode {
    Fly,        // WASD + mouse look
    Orbit,      // Orbit around selection
    Pan         // Click + drag to pan
};

/**
 * EditorApp — Full-featured editor application.
 *
 * Usage:
 *   EditorApp app;
 *   app.Run();
 *
 * The editor provides:
 * - Scene editing with gizmos
 * - Object selection and manipulation
 * - Level save/load
 * - Runtime preview mode
 */
class EditorApp : public Subsystem {
public:
    EditorApp() = default;
    ~EditorApp() override = default;

    // Subsystem interface
    bool Create() override;
    bool Update(float deltaTime) override;
    void PreRender() override;
    void PostRender() override;
    void Shutdown() override;
    SubsystemPriority GetPriority() const override { return SubsystemPriority::Editor; }

    /**
     * Run the editor main loop.
     * @return Exit code (0 = success)
     */
    int Run();

    /**
     * Process SDL event.
     * @return true if event was consumed by editor
     */
    bool ProcessEvent(const SDL_Event& event);

    // =========================================================================
    // Editor State
    // =========================================================================

    /**
     * Check if editor is in play mode (runtime preview).
     */
    bool IsPlayMode() const { return m_playMode; }

    /**
     * Enter play mode (simulate runtime).
     */
    void EnterPlayMode();

    /**
     * Exit play mode (return to editing).
     */
    void ExitPlayMode();

    /**
     * Toggle play mode.
     */
    void TogglePlayMode();

    // =========================================================================
    // Selection
    // =========================================================================

    /**
     * Get currently selected object ID (-1 if none).
     */
    int GetSelectedObject() const { return m_selectedObject; }

    /**
     * Select an object by ID.
     */
    void SelectObject(int objectId);

    /**
     * Clear selection.
     */
    void ClearSelection();

    // =========================================================================
    // Camera
    // =========================================================================

    /**
     * Get editor camera.
     */
    Camera* GetCamera() const { return m_camera.get(); }

    /**
     * Set camera mode.
     */
    void SetCameraMode(EditorCameraMode mode) { m_cameraMode = mode; }

    /**
     * Focus camera on selected object.
     */
    void FocusOnSelection();

private:
    // Engine integration
    Engine* m_engine = nullptr;

    // Editor systems
    std::unique_ptr<EditorLayer> m_editorLayer;
    std::unique_ptr<Camera> m_camera;

    // State
    bool m_playMode = false;
    int m_selectedObject = -1;
    EditorCameraMode m_cameraMode = EditorCameraMode::Fly;

    // UI state
    bool m_showHierarchy = true;
    bool m_showInspector = true;
    bool m_showViewport = true;
    bool m_showDebugOverlay = true;

    // Internal methods
    void UpdateEditorUI();
    void UpdateGizmos();
    void HandleShortcuts();
    void SaveSceneState();
    void RestoreSceneState();
};

#endif // EDITOR_BUILD
