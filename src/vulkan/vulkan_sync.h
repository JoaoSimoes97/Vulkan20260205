#pragma once

#include <vector>
#include <vulkan/vulkan.h>

/*
 * Per-frame-in-flight sync: fences and semaphores (image available, render finished).
 * Used in DrawFrame: wait fence, acquire (signal image available), submit (wait image available,
 * signal render finished + fence), present (wait render finished). Advance frame index each frame.
 * Render-finished semaphores are per swapchain image (indexed by acquired imageIndex) to satisfy
 * Vulkan semaphore reuse rules; image-available and fences remain per frame-in-flight.
 */
class VulkanSync {
public:
    VulkanSync() = default;
    ~VulkanSync();

    void Create(VkDevice device, uint32_t maxFramesInFlight, uint32_t swapchainImageCount);
    void Destroy();

    uint32_t GetCurrentFrameIndex() const { return m_currentFrame; }
    void AdvanceFrame() {
        if (m_maxFramesInFlight == 0) return;
        m_currentFrame = (m_currentFrame + 1) % m_maxFramesInFlight;
    }

    VkFence GetInFlightFence(uint32_t frameIndex) const;
    VkSemaphore GetImageAvailableSemaphore(uint32_t frameIndex) const;
    /** Per swapchain image; use the acquired imageIndex, not frame index. */
    VkSemaphore GetRenderFinishedSemaphore(uint32_t imageIndex) const;

    uint32_t GetMaxFramesInFlight() const { return m_maxFramesInFlight; }
    bool IsValid() const { return m_inFlightFences.empty() == false; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    uint32_t m_maxFramesInFlight = 0;
    uint32_t m_currentFrame = 0;
    std::vector<VkFence> m_inFlightFences;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
};
