/*
 * DescriptorSetLayoutManager — create and cache VkDescriptorSetLayout by key; store bindings for pool sizing.
 */
#include "descriptor_set_layout_manager.h"
#include "vulkan/vulkan_utils.h"
#include <algorithm>
#include <unordered_map>

void DescriptorSetLayoutManager::SetDevice(VkDevice device) {
    m_device = device;
}

VkDescriptorSetLayout DescriptorSetLayoutManager::RegisterLayout(
    const std::string& key,
    const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    if (m_device == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("DescriptorSetLayoutManager::RegisterLayout: device not set");
        return VK_NULL_HANDLE;
    }
    auto it = m_layouts.find(key);
    if (it != m_layouts.end())
        return it->second.layout;
    if (bindings.empty()) {
        VulkanUtils::LogErr("DescriptorSetLayoutManager::RegisterLayout: empty bindings for key '{}'", key);
        return VK_NULL_HANDLE;
    }
    VkDescriptorSetLayoutCreateInfo createInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext       = nullptr,
        .flags       = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings   = bindings.data(),
    };
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult r = vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &layout);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateDescriptorSetLayout failed for key '{}': {}", key, static_cast<int>(r));
        return VK_NULL_HANDLE;
    }
    Entry e;
    e.layout = layout;
    e.bindings = bindings;
    m_layouts[key] = std::move(e);
    return layout;
}

VkDescriptorSetLayout DescriptorSetLayoutManager::GetLayout(const std::string& key) const {
    auto it = m_layouts.find(key);
    if (it == m_layouts.end())
        return VK_NULL_HANDLE;
    return it->second.layout;
}

const std::vector<VkDescriptorSetLayoutBinding>* DescriptorSetLayoutManager::GetBindings(const std::string& key) const {
    auto it = m_layouts.find(key);
    if (it == m_layouts.end())
        return nullptr;
    return &it->second.bindings;
}

void DescriptorSetLayoutManager::AggregatePoolSizes(const std::vector<std::string>& keys,
                                                   uint32_t maxSets,
                                                   std::vector<VkDescriptorPoolSize>& outPoolSizes) const {
    std::unordered_map<VkDescriptorType, uint32_t> maxPerSetByType;
    for (const auto& key : keys) {
        const auto* bindings = GetBindings(key);
        if (!bindings)
            continue;
        std::unordered_map<VkDescriptorType, uint32_t> sumByType;
        for (const auto& b : *bindings) {
            sumByType[b.descriptorType] += b.descriptorCount;
        }
        for (const auto& pair : sumByType) {
            uint32_t& m = maxPerSetByType[pair.first];
            m = std::max(m, pair.second);
        }
    }
    outPoolSizes.clear();
    outPoolSizes.reserve(maxPerSetByType.size());
    for (const auto& pair : maxPerSetByType) {
        outPoolSizes.push_back(VkDescriptorPoolSize{
            .type            = pair.first,
            .descriptorCount = maxSets * pair.second,
        });
    }
}

void DescriptorSetLayoutManager::Destroy() {
    if (m_device == VK_NULL_HANDLE)
        return;
    for (auto& pair : m_layouts) {
        if (pair.second.layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_device, pair.second.layout, nullptr);
            pair.second.layout = VK_NULL_HANDLE;
        }
    }
    m_layouts.clear();
    m_device = VK_NULL_HANDLE;
}
