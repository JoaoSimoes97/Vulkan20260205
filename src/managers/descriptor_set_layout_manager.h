#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

/**
 * Registry of descriptor set layouts by key. Pipeline layouts and descriptor
 * pools are driven by layout keys. RegisterLayout() creates and caches
 * VkDescriptorSetLayout; bindings are stored for pool sizing.
 */
class DescriptorSetLayoutManager {
public:
    DescriptorSetLayoutManager() = default;

    void SetDevice(VkDevice device);

    /**
     * Register a layout by key. Bindings are copied; layout is created and cached.
     * Idempotent: if key exists, returns existing layout. Returns VK_NULL_HANDLE on failure.
     */
    VkDescriptorSetLayout RegisterLayout(const std::string& key,
                                         const std::vector<VkDescriptorSetLayoutBinding>& bindings);

    VkDescriptorSetLayout GetLayout(const std::string& key) const;

    /** Get bindings for a key (for pool size aggregation). Returns nullptr if key not found. */
    const std::vector<VkDescriptorSetLayoutBinding>* GetBindings(const std::string& key) const;

    /**
     * Aggregate VkDescriptorPoolSize for the given layout keys so that up to maxSets
     * sets can be allocated. For each descriptor type, count = maxSets * (max over keys
     * of total descriptorCount for that type in one set).
     */
    void AggregatePoolSizes(const std::vector<std::string>& keys,
                           uint32_t maxSets,
                           std::vector<VkDescriptorPoolSize>& outPoolSizes) const;

    void Destroy();

private:
    VkDevice m_device = VK_NULL_HANDLE;
    struct Entry {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        std::vector<VkDescriptorSetLayoutBinding> bindings;
    };
    std::unordered_map<std::string, Entry> m_layouts;
};
