/*
 * DescriptorPoolManager — build pool from layout keys, allocate/free sets. Main thread only.
 */
#include "descriptor_pool_manager.h"
#include "vulkan/vulkan_utils.h"

void DescriptorPoolManager::SetDevice(VkDevice device) {
    m_device = device;
}

void DescriptorPoolManager::SetLayoutManager(DescriptorSetLayoutManager* pLayoutManager) {
    m_pLayoutManager = pLayoutManager;
}

bool DescriptorPoolManager::BuildPool(const std::vector<std::string>& layoutKeys, uint32_t maxSets) {
    if (m_device == VK_NULL_HANDLE || m_pLayoutManager == nullptr) {
        VulkanUtils::LogErr("DescriptorPoolManager::BuildPool: device or layout manager not set");
        return false;
    }
    Destroy();
    std::vector<VkDescriptorPoolSize> poolSizes;
    m_pLayoutManager->AggregatePoolSizes(layoutKeys, maxSets, poolSizes);
    if (poolSizes.empty()) {
        VulkanUtils::LogErr("DescriptorPoolManager::BuildPool: no pool sizes from layout keys");
        return false;
    }
    VkDescriptorPoolCreateInfo createInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets      = maxSets,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes   = poolSizes.data(),
    };
    VkResult r = vkCreateDescriptorPool(m_device, &createInfo, nullptr, &m_pool);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateDescriptorPool failed: {}", static_cast<int>(r));
        return false;
    }
    return true;
}

VkDescriptorSet DescriptorPoolManager::AllocateSet(const std::string& layoutKey) {
    if (m_pool == VK_NULL_HANDLE || m_pLayoutManager == nullptr)
        return VK_NULL_HANDLE;
    VkDescriptorSetLayout layout = m_pLayoutManager->GetLayout(layoutKey);
    if (layout == VK_NULL_HANDLE)
        return VK_NULL_HANDLE;
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext              = nullptr,
        .descriptorPool     = m_pool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &layout,
    };
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkResult r = vkAllocateDescriptorSets(m_device, &allocInfo, &set);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkAllocateDescriptorSets failed for layout '{}': {}", layoutKey, static_cast<int>(r));
        return VK_NULL_HANDLE;
    }
    return set;
}

void DescriptorPoolManager::FreeSet(VkDescriptorSet set) {
    if (m_device == VK_NULL_HANDLE || m_pool == VK_NULL_HANDLE || set == VK_NULL_HANDLE)
        return;
    vkFreeDescriptorSets(m_device, m_pool, 1, &set);
}

void DescriptorPoolManager::Destroy() {
    if (m_device == VK_NULL_HANDLE)
        return;
    if (m_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
    }
    /* Do not clear m_device: BuildPool() calls Destroy() before creating a new pool, and needs the device. */
}
