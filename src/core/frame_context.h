/**
 * FrameContext — Per-frame resources container for triple-buffered rendering.
 *
 * Each frame in flight has its own set of resources to avoid CPU/GPU race conditions.
 * The engine maintains MAX_FRAMES_IN_FLIGHT FrameContext instances and cycles through them.
 *
 * Phase 4.1: Ring-Buffered GPU Resources
 */

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

/**
 * FrameContext — Contains all per-frame GPU resources.
 *
 * This struct consolidates resources that need to be isolated per frame:
 * - Command buffer for this frame
 * - Synchronization primitives (fence, semaphores)
 * - References to ring buffer slots for object data, lights, etc.
 *
 * Usage:
 *   FrameContext& frame = m_frameContexts[m_currentFrame];
 *   vkWaitForFences(device, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);
 *   // ... record commands into frame.commandBuffer ...
 *   // ... submit with frame.imageAvailableSemaphore / frame.renderFinishedSemaphore ...
 */
struct FrameContext {
    /* === Synchronization === */
    VkFence         inFlightFence         = VK_NULL_HANDLE;   ///< Wait before reusing this frame's resources
    VkSemaphore     imageAvailableSemaphore = VK_NULL_HANDLE; ///< Signaled when swapchain image is available
    VkSemaphore     renderFinishedSemaphore = VK_NULL_HANDLE; ///< Signaled when rendering is complete

    /* === Command Recording === */
    VkCommandBuffer commandBuffer         = VK_NULL_HANDLE;   ///< Primary command buffer for this frame

    /* === Ring Buffer Indices === */
    uint32_t        objectBufferOffset    = 0;   ///< Byte offset into object SSBO ring buffer
    uint32_t        lightBufferOffset     = 0;   ///< Byte offset into light SSBO ring buffer

    /* === Frame Statistics === */
    uint32_t        drawCallCount         = 0;   ///< Number of draw calls recorded this frame
    uint32_t        triangleCount         = 0;   ///< Number of triangles rendered this frame
    uint32_t        objectsCulled         = 0;   ///< Number of objects culled by frustum
    float           gpuTimeMs             = 0.f; ///< GPU time for this frame (if query pool available)

    /* === Frame State === */
    uint32_t        imageIndex            = 0;   ///< Swapchain image index acquired for this frame
    bool            valid                 = false; ///< True if frame context has been initialized
};

/**
 * FrameContextManager — Manages the array of FrameContext instances.
 *
 * Handles creation, destruction, and cycling through frame contexts.
 * Owns synchronization primitives and command buffers for all frames in flight.
 */
class FrameContextManager {
public:
    FrameContextManager() = default;
    ~FrameContextManager() = default;

    // Non-copyable, non-movable (owns Vulkan resources)
    FrameContextManager(const FrameContextManager&) = delete;
    FrameContextManager& operator=(const FrameContextManager&) = delete;
    FrameContextManager(FrameContextManager&&) = delete;
    FrameContextManager& operator=(FrameContextManager&&) = delete;

    /**
     * Create frame contexts with synchronization primitives and command buffers.
     * Creates its own command pool from the specified queue family.
     * @param device Vulkan device
     * @param queueFamilyIndex Queue family index for command pool creation
     * @param framesInFlight Number of frames to buffer (typically 2 or 3)
     * @return true on success
     */
    bool Create(VkDevice device, uint32_t queueFamilyIndex, uint32_t framesInFlight);

    /**
     * Destroy all frame contexts and release Vulkan resources.
     */
    void Destroy();

    /**
     * Get the current frame context.
     */
    FrameContext& GetCurrentFrame() { return m_frames[m_currentFrameIndex]; }
    const FrameContext& GetCurrentFrame() const { return m_frames[m_currentFrameIndex]; }

    /**
     * Get frame context by index.
     */
    FrameContext& GetFrame(uint32_t index) { return m_frames[index]; }
    const FrameContext& GetFrame(uint32_t index) const { return m_frames[index]; }

    /**
     * Advance to the next frame (call after presenting).
     */
    void AdvanceFrame() { m_currentFrameIndex = (m_currentFrameIndex + 1) % m_framesInFlight; }

    /**
     * Get current frame index (0 to framesInFlight-1).
     */
    uint32_t GetCurrentFrameIndex() const { return m_currentFrameIndex; }

    /**
     * Get number of frames in flight.
     */
    uint32_t GetFramesInFlight() const { return m_framesInFlight; }

    /**
     * Wait for all frames to complete (call before cleanup).
     */
    void WaitAll(VkDevice device);

private:
    VkDevice                    m_device         = VK_NULL_HANDLE;
    VkCommandPool               m_commandPool    = VK_NULL_HANDLE;
    std::vector<FrameContext>   m_frames;
    uint32_t                    m_framesInFlight = 0;
    uint32_t                    m_currentFrameIndex = 0;
};
