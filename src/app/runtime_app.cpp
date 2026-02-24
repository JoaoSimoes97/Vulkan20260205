/**
 * RuntimeApp — Implementation.
 *
 * Phase 4.4: App Separation
 */

#include "runtime_app.h"
#include "../runtime/runtime_overlay.h"
#include "../managers/scene_manager.h"

bool RuntimeApp::Create() {
    // Runtime initialization
    // Most systems are initialized by Engine
    
    m_running = true;
    return true;
}

bool RuntimeApp::Update(float deltaTime) {
    (void)deltaTime;
    
    // Check for exit conditions
    if (m_exitRequested) {
        m_running = false;
        return false;
    }

    // Game logic updates are handled by scene systems
    // RuntimeApp just orchestrates the flow
    return true;
}

void RuntimeApp::PostRender() {
    // Render minimal debug overlay
    if (m_showOverlay) {
        RenderOverlay();
    }
}

void RuntimeApp::Shutdown() {
    m_running = false;
    m_currentLevel.clear();
}

int RuntimeApp::Run(const std::string& levelPath) {
    // Set initial level if provided
    if (!levelPath.empty()) {
        m_initialLevel = levelPath;
    }

    // Load initial level
    if (!m_initialLevel.empty()) {
        if (!LoadLevel(m_initialLevel)) {
            return 1; // Failed to load level
        }
    }

    // Main loop is handled by Engine
    // RuntimeApp provides callbacks and state management
    
    return 0;
}

bool RuntimeApp::LoadLevel(const std::string& levelPath) {
    if (levelPath.empty()) {
        return false;
    }

    // Level loading is handled by SceneManager
    // RuntimeApp just tracks the current level
    
    m_currentLevel = levelPath;
    
    // Actual loading delegated to SceneManager through Engine
    // Future: Engine->GetSceneManager()->LoadLevel(levelPath)
    
    return true;
}

void RuntimeApp::RenderOverlay() {
    // RuntimeOverlay handles the actual ImGui rendering
    // This is called from PostRender()
    
    // The RuntimeOverlay class shows:
    // - FPS counter
    // - Frame time
    // - GPU memory usage (optional)
}
