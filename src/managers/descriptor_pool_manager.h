#pragma once

#include "descriptor_set_layout_manager.h"
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

/**
 * Builds and owns a VkDescriptorPool sized from layout keys. Use after
 * DescriptorSetLayoutManager has registered those layouts. All Vulkan
 * descriptor set allocation happens on the thread that owns the device
 * (main/render thread); pool build and allocate are not thread-safe.
 */
class DescriptorPoolManager {
public:
    DescriptorPoolManager() = default;

    void SetDevice(VkDevice device);
    void SetLayoutManager(DescriptorSetLayoutManager* pLayoutManager);

    /**
     * Build a new pool for the given layout keys, supporting up to maxSets total sets.
     * Destroys any existing pool. Call after layouts are registered.
     */
    bool BuildPool(const std::vector<std::string>& layoutKeys, uint32_t maxSets);

    /**
     * Allocate one descriptor set for the given layout key. Returns VK_NULL_HANDLE on failure.
     */
    VkDescriptorSet AllocateSet(const std::string& layoutKey);

    /** Free a set (returns it to the pool). Call when the set is no longer needed. */
    void FreeSet(VkDescriptorSet set);

    VkDescriptorPool GetPool() const { return m_pool; }
    bool IsValid() const { return m_pool != VK_NULL_HANDLE; }

    void Destroy();

private:
    VkDevice m_device = VK_NULL_HANDLE;
    DescriptorSetLayoutManager* m_pLayoutManager = nullptr;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
};
