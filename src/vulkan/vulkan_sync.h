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

    void Create(VkDevice pDevice_ic, uint32_t lMaxFramesInFlight_ic, uint32_t lSwapchainImageCount_ic);
    void Destroy();

    uint32_t GetCurrentFrameIndex() const { return this->m_currentFrame; }
    void AdvanceFrame() {
        if (this->m_maxFramesInFlight == 0) return;
        this->m_currentFrame = (this->m_currentFrame + 1) % this->m_maxFramesInFlight;
    }

    VkFence GetInFlightFence(uint32_t lFrameIndex_ic) const;
    /** Pointer to all in-flight fences (size = GetMaxFramesInFlight()). Wait for all before destroying trimmed resources. */
    const VkFence* GetInFlightFencePtr() const { return this->m_inFlightFences.data(); }
    VkSemaphore GetImageAvailableSemaphore(uint32_t lFrameIndex_ic) const;
    /** Per swapchain image; use the acquired imageIndex, not frame index. */
    VkSemaphore GetRenderFinishedSemaphore(uint32_t lImageIndex_ic) const;

    uint32_t GetMaxFramesInFlight() const { return this->m_maxFramesInFlight; }
    bool IsValid() const { return (this->m_inFlightFences.empty() == false); }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    uint32_t m_maxFramesInFlight = 0;
    uint32_t m_currentFrame = 0;
    std::vector<VkFence> m_inFlightFences;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
};
