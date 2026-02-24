/**
 * EditorApp — Implementation.
 *
 * Phase 4.4: App Separation
 */

#if EDITOR_BUILD

#include "editor_app.h"
#include "../editor/editor_layer.h"
#include "../camera/camera.h"

bool EditorApp::Create() {
    // Create editor camera
    m_camera = std::make_unique<Camera>();
    m_camera->SetPosition(0.0f, 2.0f, 5.0f);
    // Projection and view are updated by the rendering system

    // EditorLayer is created and managed by VulkanApp for now
    // This will be moved here in future refactoring

    return true;
}

bool EditorApp::Update(float deltaTime) {
    if (m_playMode) {
        // In play mode, let the game systems handle updates
        // Editor camera is inactive
        return true;
    }

    // Update editor camera
    if (m_camera) {
        // Camera input is handled by VulkanApp input system for now
    }

    // Handle editor shortcuts
    HandleShortcuts();
    
    (void)deltaTime;
    return true;
}

void EditorApp::PreRender() {
    // Update gizmos before rendering
    if (!m_playMode) {
        UpdateGizmos();
    }
}

void EditorApp::PostRender() {
    // Editor UI is rendered after scene
    if (!m_playMode) {
        UpdateEditorUI();
    }
}

void EditorApp::Shutdown() {
    m_camera.reset();
    m_editorLayer.reset();
}

int EditorApp::Run() {
    // Main loop is handled by Engine
    // EditorApp just provides configuration and callbacks
    
    // This method exists for standalone editor usage pattern
    // Currently returns immediately as Engine manages the loop
    return 0;
}

bool EditorApp::ProcessEvent(const SDL_Event& event) {
    if (m_playMode) {
        // In play mode, don't consume events (let game handle them)
        return false;
    }

    // Process editor-specific events
    // ImGui handles most events through imgui_impl_sdl3
    (void)event;
    return false;
}

void EditorApp::EnterPlayMode() {
    if (m_playMode) return;
    
    // Save current scene state for restoration
    SaveSceneState();
    
    m_playMode = true;
}

void EditorApp::ExitPlayMode() {
    if (!m_playMode) return;
    
    // Restore scene to pre-play state
    RestoreSceneState();
    
    m_playMode = false;
}

void EditorApp::TogglePlayMode() {
    if (m_playMode) {
        ExitPlayMode();
    } else {
        EnterPlayMode();
    }
}

void EditorApp::SelectObject(int objectId) {
    m_selectedObject = objectId;
}

void EditorApp::ClearSelection() {
    m_selectedObject = -1;
}

void EditorApp::FocusOnSelection() {
    if (m_selectedObject < 0 || !m_camera) {
        return;
    }

    // Focus camera on selected object
    // Implementation requires access to scene transforms
    // Handled by EditorLayer for now
}

void EditorApp::UpdateEditorUI() {
    // EditorLayer handles all ImGui rendering
    // This is called from EditorLayer::Render()
}

void EditorApp::UpdateGizmos() {
    // ImGuizmo gizmo updates
    // Handled by EditorLayer for now
}

void EditorApp::HandleShortcuts() {
    // Keyboard shortcuts
    // F5: Toggle play mode
    // F: Focus on selection
    // Delete: Delete selection
    // Ctrl+S: Save level
    // Handled by input system + EditorLayer
}

void EditorApp::SaveSceneState() {
    // Save a snapshot of scene state before play mode
    // Allows restoration when exiting play mode
}

void EditorApp::RestoreSceneState() {
    // Restore scene state saved before play mode
}

#endif // EDITOR_BUILD
