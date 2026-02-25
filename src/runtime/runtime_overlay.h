/*
 * RuntimeOverlay - Minimal ImGui overlay for Release builds.
 * 
 * Shows essential runtime stats:
 * - FPS counter
 * - Frame time (ms)
 * - Memory usage (optional)
 * - GPU info (optional)
 * - Level selector
 * 
 * This is the only UI rendered in Release builds.
 * For full editor functionality, use Debug builds.
 */
#pragma once

#include "ui/imgui_base.h"
#include "scene/level_selector.h"
#include <cstdint>
#include <string>

struct VulkanConfig;
class Camera;

/**
 * Runtime render statistics for overlay display.
 */
struct RenderStats {
    uint32_t drawCalls       = 0;  // Number of draw calls (batches)
    uint32_t objectsVisible  = 0;  // Objects after frustum culling
    uint32_t objectsTotal    = 0;  // Total objects in scene
    uint32_t triangles       = 0;  // Total triangles rendered
    uint32_t vertices        = 0;  // Total vertices rendered
    uint32_t batches         = 0;  // Number of batches
    float    cullingRatio    = 0;  // % culled (1.0 = all visible)
    
    // GPU culling statistics (from compute shader)
    uint32_t gpuCulledVisible = 0;   // Objects visible per GPU culler
    uint32_t gpuCulledTotal   = 0;   // Total objects submitted to GPU culler
    bool     gpuCullerActive  = false;  // Whether GPU culler is running
    bool     gpuCpuMismatch   = false;  // GPU visible != CPU visible counts
    
    // Instance tier statistics
    uint32_t instancesStatic     = 0;  // Tier 0: GPU-resident, never moves
    uint32_t instancesSemiStatic = 0;  // Tier 1: Dirty flag updates
    uint32_t instancesDynamic    = 0;  // Tier 2: Per-frame updates
    uint32_t instancesProcedural = 0;  // Tier 3: Compute-generated
    
    // Draw calls per tier (batched)
    uint32_t drawCallsStatic     = 0;  // Batched static draws
    uint32_t drawCallsSemiStatic = 0;  // Batched semi-static draws
    uint32_t drawCallsDynamic    = 0;  // Batched dynamic draws
    uint32_t drawCallsProcedural = 0;  // Compute-generated draws
    
    // SSBO uploads per tier (objects updated this frame)
    uint32_t uploadsStatic       = 0;  // Tier 0: Uploads on scene rebuild only
    uint32_t uploadsSemiStatic   = 0;  // Tier 1: Uploads on dirty flag
    uint32_t uploadsDynamic      = 0;  // Tier 2: Uploads every frame
    uint32_t uploadsProcedural   = 0;  // Tier 3: Compute-generated uploads
};

/**
 * RuntimeOverlay - Lightweight stats display for Release builds.
 * 
 * Features:
 * - FPS counter with graph
 * - Frame time display
 * - Camera position/rotation
 * - Toggle visibility with F3 key
 */
class RuntimeOverlay : public ImGuiBase {
public:
    RuntimeOverlay() = default;
    ~RuntimeOverlay() override = default;

    /**
     * Initialize the runtime overlay.
     */
    void Init(
        SDL_Window* pWindow,
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        uint32_t graphicsQueueFamily,
        VkQueue graphicsQueue,
        VkRenderPass renderPass,
        uint32_t imageCount
    );

    /**
     * Update stats for current frame.
     * @param deltaTime Time since last frame in seconds.
     */
    void Update(float deltaTime);

    /**
     * Draw the overlay. Call between BeginFrame() and EndFrame().
     * @param pCamera Optional camera for position display.
     * @param pConfig Optional config for additional info.
     */
    void Draw(const Camera* pCamera = nullptr, const VulkanConfig* pConfig = nullptr);

    /**
     * Toggle overlay visibility.
     */
    void ToggleVisible() { m_bVisible = !m_bVisible; }
    bool IsVisible() const { return m_bVisible; }

    /**
     * Set overlay position (screen corner).
     * 0 = top-left, 1 = top-right, 2 = bottom-left, 3 = bottom-right
     */
    void SetCorner(int corner) { m_corner = corner; }

    /**
     * Set current frame render statistics.
     */
    void SetRenderStats(const RenderStats& stats) { m_renderStats = stats; }

    /**
     * Set level selector for level switching UI.
     */
    void SetLevelSelector(LevelSelector* pSelector) { m_pLevelSelector = pSelector; }

protected:
    void DrawContent() override;

private:
    void DrawStatsWindow(const Camera* pCamera, const VulkanConfig* pConfig);
    void DrawLevelSelector();

    bool m_bVisible = true;
    int m_corner = 1;  // Top-right by default

    // Stats tracking
    float m_deltaTime = 0.0f;
    float m_fps = 0.0f;
    float m_avgFrameTime = 0.0f;
    float m_minFrameTime = 1000.0f;
    float m_maxFrameTime = 0.0f;
    
    // FPS history for graph
    static constexpr int kFpsHistorySize = 120;
    float m_fpsHistory[kFpsHistorySize] = {};
    int m_fpsHistoryIndex = 0;
    
    // Smoothing
    static constexpr float kSmoothingFactor = 0.95f;
    
    // Temp storage for Draw() context
    const Camera* m_pCurrentCamera = nullptr;
    const VulkanConfig* m_pCurrentConfig = nullptr;
    
    // Render statistics
    RenderStats m_renderStats;  
    
    // Level selector (optional, owned externally)
    LevelSelector* m_pLevelSelector = nullptr;
};
