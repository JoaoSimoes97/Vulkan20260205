/**
 * Renderer — High-level render orchestration.
 *
 * Manages render passes, command buffer recording, and frame presentation.
 * Extracts rendering logic from VulkanApp for cleaner separation.
 *
 * Responsibilities:
 * - Swapchain management (acquire/present)
 * - Command buffer recording
 * - Render pass orchestration (scene, debug, UI)
 * - Frame synchronization
 *
 * Phase 4.3: Renderer Extraction
 */

#pragma once

#include "render_context.h"
#include "gpu_buffer.h"
#include "core/frame_context.h"
#include <vulkan/vulkan.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

// Forward declarations
class Scene;
struct DrawCall;
class BatchedDrawList;
class Camera;

/**
 * FrameData — Per-frame rendering data.
 */
struct FrameData {
    VkCommandBuffer     commandBuffer       = VK_NULL_HANDLE;
    VkFence             inFlightFence       = VK_NULL_HANDLE;
    VkSemaphore         imageAvailableSem   = VK_NULL_HANDLE;
    VkSemaphore         renderFinishedSem   = VK_NULL_HANDLE;
    uint32_t            imageIndex          = 0;
    
    // Per-frame GPU buffers (ring-buffered)
    VkDeviceSize        objectBufferOffset  = 0;
    VkDeviceSize        lightBufferOffset   = 0;
};

/**
 * RenderStats — Frame rendering statistics.
 */
struct RenderStats {
    uint32_t    drawCalls       = 0;
    uint32_t    triangles       = 0;
    uint32_t    objectsRendered = 0;
    uint32_t    objectsCulled   = 0;
    float       gpuTimeMs       = 0.f;
    float       cpuTimeMs       = 0.f;
};

/**
 * RenderPassType — Types of render passes.
 */
enum class RenderPassType : uint32_t {
    Scene,      // Main scene rendering
    Debug,      // Debug visualization (light gizmos, wireframes)
    UI,         // ImGui overlay
    COUNT
};

/**
 * Renderer — Orchestrates frame rendering.
 *
 * Usage:
 *   Renderer renderer;
 *   renderer.Create(context);
 *   
 *   // Each frame:
 *   if (renderer.BeginFrame()) {
 *       renderer.RenderScene(scene, camera);
 *       renderer.RenderDebug(debugData);
 *       renderer.RenderUI(uiCallback);
 *       renderer.EndFrame();
 *   }
 */
class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    // Non-copyable
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /**
     * Initialize renderer with context.
     * @param context Render context with Vulkan handles
     * @return true on success
     */
    bool Create(const RenderContext& context);

    /**
     * Cleanup renderer resources.
     */
    void Destroy();

    /**
     * Handle window resize. Triggers swapchain recreation.
     * @param width New width
     * @param height New height
     */
    void OnResize(uint32_t width, uint32_t height);

    /**
     * Begin a new frame.
     * Acquires swapchain image, waits for fence, begins command buffer.
     * @return true if frame can proceed, false if swapchain needs recreation
     */
    bool BeginFrame();

    /**
     * End current frame.
     * Ends command buffer, submits to queue, presents.
     * @return true on success, false on error (swapchain out of date, etc.)
     */
    bool EndFrame();

    /**
     * Get current command buffer for recording.
     * Only valid between BeginFrame() and EndFrame().
     */
    VkCommandBuffer GetCommandBuffer() const { return m_currentCommandBuffer; }

    /**
     * Get current frame index (0 to framesInFlight-1).
     */
    uint32_t GetCurrentFrameIndex() const { return m_currentFrame; }

    /**
     * Get acquired swapchain image index.
     */
    uint32_t GetImageIndex() const { return m_imageIndex; }

    /**
     * Get current frame's render stats.
     */
    const RenderStats& GetStats() const { return m_stats; }

    /**
     * Set frames in flight (1-3).
     * Must be called before Create() or after Destroy().
     */
    void SetFramesInFlight(uint32_t count) { m_framesInFlight = count; }

    /**
     * Begin main render pass.
     * @param clearColor RGBA clear color
     */
    void BeginMainRenderPass(float clearR = 0.1f, float clearG = 0.1f, float clearB = 0.1f, float clearA = 1.f);

    /**
     * End main render pass.
     */
    void EndRenderPass();

    /**
     * Check if currently inside a render pass.
     */
    bool IsInRenderPass() const { return m_inRenderPass; }

    /**
     * Get render context.
     */
    const RenderContext& GetContext() const { return m_context; }

    /**
     * Check if swapchain needs recreation.
     */
    bool NeedsSwapchainRecreation() const { return m_needsRecreation; }

    /**
     * Recreate swapchain and dependent resources.
     * @param newWidth New width (0 = use current surface size)
     * @param newHeight New height (0 = use current surface size)
     * @return true on success
     */
    bool RecreateSwapchain(uint32_t newWidth = 0, uint32_t newHeight = 0);

private:
    bool CreateFrameResources();
    void DestroyFrameResources();
    bool CreateRenderPass();
    void DestroyRenderPass();
    bool CreateFramebuffers();
    void DestroyFramebuffers();
    bool CreateDepthResources();
    void DestroyDepthResources();

    // Context (non-owning)
    RenderContext m_context;

    // Frame resources
    std::vector<FrameData>      m_frames;
    uint32_t                    m_framesInFlight = 2;
    uint32_t                    m_currentFrame = 0;
    uint32_t                    m_imageIndex = 0;
    VkCommandBuffer             m_currentCommandBuffer = VK_NULL_HANDLE;

    // Swapchain resources
    std::vector<VkImageView>    m_swapchainImageViews;
    std::vector<VkFramebuffer>  m_framebuffers;

    // Depth buffer
    VkImage                     m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory              m_depthMemory = VK_NULL_HANDLE;
    VkImageView                 m_depthImageView = VK_NULL_HANDLE;

    // Render pass (owned)
    VkRenderPass                m_renderPass = VK_NULL_HANDLE;

    // Command pool (owned)
    VkCommandPool               m_commandPool = VK_NULL_HANDLE;

    // State
    bool                        m_initialized = false;
    bool                        m_inRenderPass = false;
    bool                        m_needsRecreation = false;
    RenderStats                 m_stats;
};
