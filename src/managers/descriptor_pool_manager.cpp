/*
 * DescriptorPoolManager — dynamic pool with growth. Main thread only.
 */
#include "descriptor_pool_manager.h"
#include "vulkan/vulkan_utils.h"

void DescriptorPoolManager::SetDevice(VkDevice device) {
    m_device = device;
}

void DescriptorPoolManager::SetLayoutManager(DescriptorSetLayoutManager* pLayoutManager) {
    m_pLayoutManager = pLayoutManager;
}

void DescriptorPoolManager::SetDeviceLimit(uint32_t maxSets) {
    m_deviceLimit = maxSets;
    VulkanUtils::LogInfo("DescriptorPoolManager: device limit set to {} descriptor sets", maxSets);
}

bool DescriptorPoolManager::BuildPool(const std::vector<std::string>& layoutKeys, uint32_t initialCapacity) {
    if (m_device == VK_NULL_HANDLE || m_pLayoutManager == nullptr) {
        VulkanUtils::LogErr("DescriptorPoolManager::BuildPool: device or layout manager not set");
        return false;
    }
    
    Destroy();
    m_layoutKeys = layoutKeys;
    m_currentCapacity = initialCapacity;
    m_allocatedCount = 0;
    m_warned75 = false;
    m_warned90 = false;
    
    // Create initial pool
    if (!CreateAdditionalPool(initialCapacity)) {
        return false;
    }
    
    m_pool = m_pools[0]; // Primary pool for compatibility
    VulkanUtils::LogInfo("DescriptorPoolManager: created initial pool with capacity {}", initialCapacity);
    return true;
}

bool DescriptorPoolManager::CreateAdditionalPool(uint32_t capacity) {
    if (m_device == VK_NULL_HANDLE || m_pLayoutManager == nullptr) {
        return false;
    }
    
    std::vector<VkDescriptorPoolSize> poolSizes;
    m_pLayoutManager->AggregatePoolSizes(m_layoutKeys, capacity, poolSizes);
    if (poolSizes.empty()) {
        VulkanUtils::LogErr("DescriptorPoolManager: no pool sizes from layout keys");
        return false;
    }
    
    VkDescriptorPoolCreateInfo createInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets      = capacity,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes   = poolSizes.data(),
    };
    
    VkDescriptorPool newPool = VK_NULL_HANDLE;
    VkResult r = vkCreateDescriptorPool(m_device, &createInfo, nullptr, &newPool);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateDescriptorPool failed: {}", static_cast<int>(r));
        return false;
    }
    
    m_pools.push_back(newPool);
    return true;
}

void DescriptorPoolManager::CheckCapacityWarnings() {
    float usage = GetUsagePercent();
    
    if (!m_warned75 && usage >= 75.0f) {
        VulkanUtils::LogWarn("DescriptorPoolManager: 75% capacity ({}/{})", m_allocatedCount, m_currentCapacity);
        m_warned75 = true;
    }
    
    if (!m_warned90 && usage >= 90.0f) {
        VulkanUtils::LogWarn("DescriptorPoolManager: 90% capacity ({}/{})", m_allocatedCount, m_currentCapacity);
        m_warned90 = true;
    }
}

VkDescriptorSet DescriptorPoolManager::AllocateSet(const std::string& layoutKey) {
    if (m_pools.empty() || m_pLayoutManager == nullptr)
        return VK_NULL_HANDLE;
    
    VkDescriptorSetLayout layout = m_pLayoutManager->GetLayout(layoutKey);
    if (layout == VK_NULL_HANDLE)
        return VK_NULL_HANDLE;
    
    // Try allocating from existing pools (newest first, as they're less likely to be full)
    for (auto it = m_pools.rbegin(); it != m_pools.rend(); ++it) {
        VkDescriptorSetAllocateInfo allocInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext              = nullptr,
            .descriptorPool     = *it,
            .descriptorSetCount = 1,
            .pSetLayouts        = &layout,
        };
        
        VkDescriptorSet set = VK_NULL_HANDLE;
        VkResult r = vkAllocateDescriptorSets(m_device, &allocInfo, &set);
        
        if (r == VK_SUCCESS) {
            ++m_allocatedCount;
            CheckCapacityWarnings();
            return set;
        }
        
        // If pool is exhausted (VK_ERROR_OUT_OF_POOL_MEMORY), try next pool
        if (r != VK_ERROR_OUT_OF_POOL_MEMORY && r != VK_ERROR_FRAGMENTED_POOL) {
            VulkanUtils::LogErr("vkAllocateDescriptorSets failed for layout '{}': {}", layoutKey, static_cast<int>(r));
            return VK_NULL_HANDLE;
        }
    }
    
    // All pools exhausted - create new pool with double capacity
    uint32_t newCapacity = m_currentCapacity * 2;
    
    if (newCapacity > m_deviceLimit) {
        newCapacity = m_deviceLimit;
    }
    
    if (m_currentCapacity >= m_deviceLimit) {
        VulkanUtils::LogErr("DescriptorPoolManager: cannot grow beyond device limit of {}", m_deviceLimit);
        return VK_NULL_HANDLE;
    }
    
    VulkanUtils::LogWarn("DescriptorPoolManager: growing from {} to {} sets", m_currentCapacity, newCapacity);
    
    if (!CreateAdditionalPool(newCapacity)) {
        VulkanUtils::LogErr("DescriptorPoolManager: failed to create additional pool");
        return VK_NULL_HANDLE;
    }
    
    m_currentCapacity += newCapacity;
    m_warned75 = false; // Reset warnings for new capacity
    m_warned90 = false;
    
    // Allocate from the new pool
    VkDescriptorPool newPool = m_pools.back();
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext              = nullptr,
        .descriptorPool     = newPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &layout,
    };
    
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkResult r = vkAllocateDescriptorSets(m_device, &allocInfo, &set);
    
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkAllocateDescriptorSets failed after pool growth: {}", static_cast<int>(r));
        return VK_NULL_HANDLE;
    }
    
    ++m_allocatedCount;
    return set;
}

void DescriptorPoolManager::FreeSet(VkDescriptorSet set) {
    if (m_device == VK_NULL_HANDLE || set == VK_NULL_HANDLE)
        return;
    
    // We don't know which pool the set came from, so we try to free from each
    // (Vulkan will fail silently if wrong pool, which is fine)
    for (VkDescriptorPool pool : m_pools) {
        vkFreeDescriptorSets(m_device, pool, 1, &set);
    }
    
    if (m_allocatedCount > 0) {
        --m_allocatedCount;
    }
}

void DescriptorPoolManager::Destroy() {
    if (m_device == VK_NULL_HANDLE)
        return;
    
    for (VkDescriptorPool pool : m_pools) {
        vkDestroyDescriptorPool(m_device, pool, nullptr);
    }
    
    m_pools.clear();
    m_pool = VK_NULL_HANDLE;
    m_currentCapacity = 0;
    m_allocatedCount = 0;
    m_warned75 = false;
    m_warned90 = false;
    
    /* Do not clear m_device: BuildPool() calls Destroy() before creating a new pool, and needs the device. */
}
