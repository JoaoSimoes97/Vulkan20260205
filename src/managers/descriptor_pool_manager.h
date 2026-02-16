#pragma once

#include "descriptor_set_layout_manager.h"
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

/**
 * Builds and owns a VkDescriptorPool with dynamic growth. Use after
 * DescriptorSetLayoutManager has registered those layouts. All Vulkan
 * descriptor set allocation happens on the thread that owns the device
 * (main/render thread); pool build and allocate are not thread-safe.
 * 
 * Features:
 * - Starts with initialCapacity, doubles when exhausted (up to device limit)
 * - Warns at 75% and 90% capacity
 * - Tracks allocated sets for growth and diagnostics
 */
class DescriptorPoolManager {
public:
    DescriptorPoolManager() = default;

    void SetDevice(VkDevice device);
    void SetLayoutManager(DescriptorSetLayoutManager* pLayoutManager);
    void SetDeviceLimit(uint32_t maxSets); // Call with VulkanDevice::GetMaxDescriptorSets()

    /**
     * Build initial pool for the given layout keys, supporting up to initialCapacity total sets.
     * Destroys any existing pool. Call after layouts are registered.
     * Pool will grow dynamically if capacity is exceeded (up to device limit).
     */
    bool BuildPool(const std::vector<std::string>& layoutKeys, uint32_t initialCapacity);

    /**
     * Allocate one descriptor set for the given layout key. Returns VK_NULL_HANDLE on failure.
     * If pool is exhausted, attempts to grow the pool automatically.
     */
    VkDescriptorSet AllocateSet(const std::string& layoutKey);

    /** Free a set (returns it to the pool). Call when the set is no longer needed. */
    void FreeSet(VkDescriptorSet set);

    VkDescriptorPool GetPool() const { return m_pool; }
    bool IsValid() const { return m_pool != VK_NULL_HANDLE; }
    
    // Diagnostics
    uint32_t GetAllocatedCount() const { return m_allocatedCount; }
    uint32_t GetCapacity() const { return m_currentCapacity; }
    float GetUsagePercent() const { return m_currentCapacity > 0 ? (100.0f * m_allocatedCount / m_currentCapacity) : 0.0f; }

    void Destroy();

private:
    bool CreateAdditionalPool(uint32_t capacity); // Create new pool, add to m_pools
    void CheckCapacityWarnings(); // Warn at 75%, 90%

    VkDevice m_device = VK_NULL_HANDLE;
    DescriptorSetLayoutManager* m_pLayoutManager = nullptr;
    
    // Multiple pools for dynamic growth (can't reallocate sets, so keep old pools alive)
    std::vector<VkDescriptorPool> m_pools;
    VkDescriptorPool m_pool = VK_NULL_HANDLE; // Current/primary pool (for compatibility)
    
    std::vector<std::string> m_layoutKeys; // Stored for growth
    uint32_t m_currentCapacity = 0;
    uint32_t m_allocatedCount = 0;
    uint32_t m_deviceLimit = 4096; // Default, set via SetDeviceLimit()
    
    bool m_warned75 = false;
    bool m_warned90 = false;
};
