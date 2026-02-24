/**
 * Engine — Core engine orchestrator.
 *
 * Phase 4.4: App Separation
 *
 * The Engine class owns and coordinates all subsystems:
 * - Window management
 * - Rendering (Renderer, ViewportManager)
 * - Scene management
 * - Resource management (textures, meshes, materials)
 * - Frame timing
 *
 * Apps (EditorApp, RuntimeApp) configure and drive the Engine.
 */

#pragma once

#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <cstdint>
#include <functional>

// Forward declarations
class Window;
class Renderer;
class SceneManager;
class MeshManager;
class TextureManager;
class MaterialManager;
class PipelineManager;
class Subsystem;

/**
 * Engine configuration passed during creation.
 */
struct EngineConfig {
    std::string windowTitle = "Vulkan Engine";
    uint32_t windowWidth = 1280;
    uint32_t windowHeight = 720;
    bool enableValidation = true;
    bool enableEditor = false;
    uint32_t framesInFlight = 2;
    
    // Path configuration
    std::string projectRoot;
    std::string configPath = "config/config.json";
    std::string shadersPath = "shaders/build";
};

/**
 * Per-frame timing information.
 */
struct FrameTiming {
    float deltaTime = 0.0f;         // Seconds since last frame
    float totalTime = 0.0f;         // Total elapsed time since start
    uint64_t frameCount = 0;        // Total frames rendered
    float fps = 0.0f;               // Frames per second (smoothed)
};

/**
 * Engine state for external queries.
 */
enum class EngineState {
    Uninitialized,
    Initializing,
    Running,
    Paused,
    ShuttingDown,
    Terminated
};

/**
 * Engine — Main engine orchestrator.
 *
 * Lifecycle:
 *   Engine engine;
 *   engine.Create(config);
 *   while (engine.IsRunning()) {
 *       engine.Update();
 *   }
 *   engine.Destroy();
 *
 * Apps should:
 * - Configure the engine before Create()
 * - Register app-specific subsystems
 * - Inject update callbacks for custom logic
 */
class Engine {
public:
    Engine() = default;
    ~Engine();

    // Non-copyable
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    /**
     * Initialize the engine with configuration.
     * Creates window, Vulkan context, and all subsystems.
     * @param config Engine configuration
     * @return true on success
     */
    bool Create(const EngineConfig& config);

    /**
     * Shutdown engine and release all resources.
     */
    void Destroy();

    /**
     * Process one frame: input, update, render.
     * @return true if engine should continue, false if quit requested
     */
    bool Update();

    /**
     * Check if engine is still running.
     */
    bool IsRunning() const { return m_state == EngineState::Running; }

    /**
     * Request engine shutdown (will exit on next Update).
     */
    void RequestQuit() { m_quitRequested = true; }

    /**
     * Get current engine state.
     */
    EngineState GetState() const { return m_state; }

    /**
     * Get frame timing information.
     */
    const FrameTiming& GetTiming() const { return m_timing; }

    /**
     * Get configuration.
     */
    const EngineConfig& GetConfig() const { return m_config; }

    // =========================================================================
    // Subsystem Access
    // =========================================================================
    // Apps can access subsystems for custom logic.
    // These return nullptr if engine not initialized.
    // =========================================================================

    Window* GetWindow() const { return m_window.get(); }
    Renderer* GetRenderer() const { return m_renderer.get(); }
    SceneManager* GetSceneManager() const { return m_sceneManager; }
    MeshManager* GetMeshManager() const { return m_meshManager; }
    TextureManager* GetTextureManager() const { return m_textureManager; }
    MaterialManager* GetMaterialManager() const { return m_materialManager; }

    // =========================================================================
    // Callback Registration
    // =========================================================================
    // Apps can register callbacks for custom per-frame logic.
    // =========================================================================

    using UpdateCallback = std::function<void(float deltaTime)>;
    using RenderCallback = std::function<void()>;

    /**
     * Register a callback to run during Update phase.
     * Multiple callbacks are called in registration order.
     */
    void AddUpdateCallback(UpdateCallback callback);

    /**
     * Register a callback to run during Render phase (after scene rendering).
     * Used for custom rendering (UI, debug overlays, etc.)
     */
    void AddRenderCallback(RenderCallback callback);

    /**
     * Register a custom subsystem to be managed by the engine.
     * The engine takes ownership and will call Shutdown() on destroy.
     */
    void RegisterSubsystem(std::unique_ptr<Subsystem> subsystem);

private:
    // Configuration
    EngineConfig m_config{};

    // State
    EngineState m_state = EngineState::Uninitialized;
    bool m_quitRequested = false;

    // Timing
    FrameTiming m_timing{};
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_lastFrameTime;
    float m_fpsAccumulator = 0.0f;
    uint32_t m_fpsFrameCount = 0;

    // Core systems (owned)
    std::unique_ptr<Window> m_window;
    std::unique_ptr<Renderer> m_renderer;

    // Managers (pointers to external, set during init)
    // These are created by VulkanApp infrastructure for now
    SceneManager* m_sceneManager = nullptr;
    MeshManager* m_meshManager = nullptr;
    TextureManager* m_textureManager = nullptr;
    MaterialManager* m_materialManager = nullptr;
    PipelineManager* m_pipelineManager = nullptr;

    // Custom subsystems
    std::vector<std::unique_ptr<Subsystem>> m_subsystems;

    // Callbacks
    std::vector<UpdateCallback> m_updateCallbacks;
    std::vector<RenderCallback> m_renderCallbacks;

    // Internal methods
    bool InitializeWindow();
    bool InitializeVulkan();
    bool InitializeSubsystems();
    void ProcessInput();
    void UpdateSystems(float deltaTime);
    void Render();
    void UpdateTiming();
};
