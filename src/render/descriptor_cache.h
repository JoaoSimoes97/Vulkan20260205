/**
 * DescriptorCache — Pre-allocated descriptor pool management.
 *
 * Phase 4.3: Renderer Extraction
 *
 * Provides per-frame descriptor set allocation with automatic reset.
 * Uses multiple pools to avoid running out of descriptors.
 */

#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

/**
 * Configuration for descriptor pool allocation.
 */
struct DescriptorPoolConfig {
    uint32_t maxSets = 1000;
    uint32_t uniformBufferCount = 500;
    uint32_t combinedSamplerCount = 500;
    uint32_t storageBufferCount = 100;
    uint32_t storageImageCount = 50;
};

/**
 * DescriptorCache — Manages descriptor pools and set allocation.
 *
 * Features:
 * - Pre-allocated pools with configurable sizes
 * - Per-frame reset (all sets returned to pool)
 * - Automatic pool switching when current pool is exhausted
 * - Thread-safe allocation (single-threaded render context assumed)
 *
 * Usage:
 *   DescriptorCache cache;
 *   cache.Create(device, config, framesInFlight);
 *   
 *   // Each frame:
 *   cache.ResetFrame(currentFrame);
 *   VkDescriptorSet set = cache.Allocate(layout);
 *   
 *   // Cleanup:
 *   cache.Destroy();
 */
class DescriptorCache {
public:
    DescriptorCache() = default;
    ~DescriptorCache();

    // Non-copyable, moveable
    DescriptorCache(const DescriptorCache&) = delete;
    DescriptorCache& operator=(const DescriptorCache&) = delete;
    DescriptorCache(DescriptorCache&& other) noexcept;
    DescriptorCache& operator=(DescriptorCache&& other) noexcept;

    /**
     * Create descriptor pools for each frame.
     * @param device Vulkan device
     * @param config Pool configuration
     * @param framesInFlight Number of frames (typically 2-3)
     * @return true on success
     */
    bool Create(VkDevice device, const DescriptorPoolConfig& config, uint32_t framesInFlight);

    /**
     * Destroy all pools and release resources.
     */
    void Destroy();

    /**
     * Reset the pool for a specific frame.
     * Call at the beginning of each frame before allocating.
     * @param frameIndex Current frame index (0 to framesInFlight-1)
     */
    void ResetFrame(uint32_t frameIndex);

    /**
     * Allocate a descriptor set from the current frame's pool.
     * @param layout Descriptor set layout
     * @return Allocated descriptor set, or VK_NULL_HANDLE on failure
     */
    VkDescriptorSet Allocate(VkDescriptorSetLayout layout);

    /**
     * Allocate multiple descriptor sets at once.
     * @param layouts Array of layouts
     * @param count Number of sets to allocate
     * @param outSets Output array for allocated sets
     * @return true if all sets were allocated
     */
    bool AllocateBatch(const VkDescriptorSetLayout* layouts, uint32_t count, VkDescriptorSet* outSets);

    /**
     * Check if cache is initialized.
     */
    bool IsValid() const { return m_device != VK_NULL_HANDLE && !m_framePools.empty(); }

    /**
     * Get pool usage statistics.
     */
    struct Stats {
        uint32_t totalPools;
        uint32_t activePoolIndex;
        uint32_t setsAllocated;
    };
    Stats GetStats() const;

private:
    struct FramePool {
        std::vector<VkDescriptorPool> pools;
        uint32_t activePoolIndex = 0;
        uint32_t setsAllocatedInActivePool = 0;
    };

    VkDevice m_device = VK_NULL_HANDLE;
    DescriptorPoolConfig m_config{};
    std::vector<FramePool> m_framePools;
    uint32_t m_currentFrame = 0;

    /**
     * Create a new descriptor pool with configured sizes.
     */
    VkDescriptorPool CreatePool();

    /**
     * Get or create a pool with available space.
     */
    VkDescriptorPool GetAvailablePool();
};
