/**
 * Engine — Implementation.
 *
 * Phase 4.4: App Separation
 */

#include "engine.h"
#include "subsystem.h"
#include "../window/window.h"
#include "../render/renderer.h"

#include <algorithm>

Engine::~Engine() {
    if (m_state != EngineState::Uninitialized && m_state != EngineState::Terminated) {
        Destroy();
    }
}

bool Engine::Create(const EngineConfig& config) {
    if (m_state != EngineState::Uninitialized) {
        return false;
    }

    m_state = EngineState::Initializing;
    m_config = config;

    // Initialize timing
    m_startTime = std::chrono::steady_clock::now();
    m_lastFrameTime = m_startTime;

    // Initialize window
    if (!InitializeWindow()) {
        m_state = EngineState::Terminated;
        return false;
    }

    // Initialize Vulkan context
    if (!InitializeVulkan()) {
        Destroy();
        return false;
    }

    // Initialize subsystems
    if (!InitializeSubsystems()) {
        Destroy();
        return false;
    }

    m_state = EngineState::Running;
    return true;
}

void Engine::Destroy() {
    if (m_state == EngineState::Uninitialized || m_state == EngineState::Terminated) {
        return;
    }

    m_state = EngineState::ShuttingDown;

    // Shutdown custom subsystems in reverse order
    for (auto it = m_subsystems.rbegin(); it != m_subsystems.rend(); ++it) {
        (*it)->Shutdown();
    }
    m_subsystems.clear();

    // Destroy renderer
    if (m_renderer) {
        m_renderer->Destroy();
        m_renderer.reset();
    }

    // Destroy window
    m_window.reset();

    // Clear callbacks
    m_updateCallbacks.clear();
    m_renderCallbacks.clear();

    // Clear manager pointers (not owned)
    m_sceneManager = nullptr;
    m_meshManager = nullptr;
    m_textureManager = nullptr;
    m_materialManager = nullptr;
    m_pipelineManager = nullptr;

    m_state = EngineState::Terminated;
}

bool Engine::Update() {
    if (m_state != EngineState::Running) {
        return false;
    }

    // Update timing
    UpdateTiming();

    // Process input events
    ProcessInput();

    // Check quit
    if (m_quitRequested) {
        return false;
    }

    // Update systems
    UpdateSystems(m_timing.deltaTime);

    // Render frame
    Render();

    m_timing.frameCount++;

    return !m_quitRequested;
}

void Engine::AddUpdateCallback(UpdateCallback callback) {
    if (callback) {
        m_updateCallbacks.push_back(std::move(callback));
    }
}

void Engine::AddRenderCallback(RenderCallback callback) {
    if (callback) {
        m_renderCallbacks.push_back(std::move(callback));
    }
}

void Engine::RegisterSubsystem(std::unique_ptr<Subsystem> subsystem) {
    if (subsystem) {
        m_subsystems.push_back(std::move(subsystem));
        
        // Sort by priority (lower values first)
        std::sort(m_subsystems.begin(), m_subsystems.end(),
            [](const auto& a, const auto& b) {
                return a->GetPriority() < b->GetPriority();
            });
    }
}

bool Engine::InitializeWindow() {
    // Window creation is delegated to app layer
    // Engine holds a pointer to externally-created Window
    // This avoids constructor parameter issues
    
    // Window is created by VulkanApp which passes it to Engine
    // For now, return true as placeholder
    return true;
}

bool Engine::InitializeVulkan() {
    // Vulkan initialization is currently handled by VulkanApp
    // This is a placeholder for future refactoring where Engine
    // will own the Vulkan context directly
    
    m_renderer = std::make_unique<Renderer>();
    return m_renderer != nullptr;
}

bool Engine::InitializeSubsystems() {
    // Initialize registered subsystems
    for (auto& subsystem : m_subsystems) {
        if (!subsystem->Create()) {
            return false;
        }
    }
    return true;
}

void Engine::ProcessInput() {
    // Input processing is currently handled by Window/SDL
    // Check if window requested close
    if (m_window) {
        // Window polling handled elsewhere for now
    }
}

void Engine::UpdateSystems(float deltaTime) {
    // Update subsystems
    for (auto& subsystem : m_subsystems) {
        subsystem->Update(deltaTime);
    }

    // Call registered update callbacks
    for (auto& callback : m_updateCallbacks) {
        callback(deltaTime);
    }
}

void Engine::Render() {
    if (!m_renderer) {
        return;
    }

    // Pre-render phase for subsystems
    for (auto& subsystem : m_subsystems) {
        subsystem->PreRender();
    }

    // Renderer BeginFrame/EndFrame is handled at app layer for now
    // since VulkanApp still owns swapchain management

    // Call registered render callbacks
    for (auto& callback : m_renderCallbacks) {
        callback();
    }

    // Post-render phase for subsystems
    for (auto& subsystem : m_subsystems) {
        subsystem->PostRender();
    }
}

void Engine::UpdateTiming() {
    auto currentTime = std::chrono::steady_clock::now();
    
    // Delta time
    std::chrono::duration<float> delta = currentTime - m_lastFrameTime;
    m_timing.deltaTime = delta.count();
    m_lastFrameTime = currentTime;

    // Total time
    std::chrono::duration<float> total = currentTime - m_startTime;
    m_timing.totalTime = total.count();

    // FPS calculation (smoothed over 1 second)
    m_fpsAccumulator += m_timing.deltaTime;
    m_fpsFrameCount++;

    if (m_fpsAccumulator >= 1.0f) {
        m_timing.fps = static_cast<float>(m_fpsFrameCount) / m_fpsAccumulator;
        m_fpsAccumulator = 0.0f;
        m_fpsFrameCount = 0;
    }
}
