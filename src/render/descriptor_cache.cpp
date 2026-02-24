/**
 * DescriptorCache — Implementation.
 *
 * Phase 4.3: Renderer Extraction
 */

#include "descriptor_cache.h"
#include <algorithm>
#include <array>

DescriptorCache::~DescriptorCache() {
    if (m_device != VK_NULL_HANDLE) {
        Destroy();
    }
}

DescriptorCache::DescriptorCache(DescriptorCache&& other) noexcept
    : m_device(other.m_device)
    , m_config(other.m_config)
    , m_framePools(std::move(other.m_framePools))
    , m_currentFrame(other.m_currentFrame) {
    other.m_device = VK_NULL_HANDLE;
}

DescriptorCache& DescriptorCache::operator=(DescriptorCache&& other) noexcept {
    if (this != &other) {
        Destroy();
        m_device = other.m_device;
        m_config = other.m_config;
        m_framePools = std::move(other.m_framePools);
        m_currentFrame = other.m_currentFrame;
        other.m_device = VK_NULL_HANDLE;
    }
    return *this;
}

bool DescriptorCache::Create(VkDevice device, const DescriptorPoolConfig& config, uint32_t framesInFlight) {
    if (device == VK_NULL_HANDLE || framesInFlight == 0) {
        return false;
    }

    m_device = device;
    m_config = config;
    m_framePools.resize(framesInFlight);

    // Create one initial pool per frame
    for (auto& framePool : m_framePools) {
        VkDescriptorPool pool = CreatePool();
        if (pool == VK_NULL_HANDLE) {
            Destroy();
            return false;
        }
        framePool.pools.push_back(pool);
        framePool.activePoolIndex = 0;
        framePool.setsAllocatedInActivePool = 0;
    }

    return true;
}

void DescriptorCache::Destroy() {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }

    for (auto& framePool : m_framePools) {
        for (auto pool : framePool.pools) {
            if (pool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(m_device, pool, nullptr);
            }
        }
    }
    m_framePools.clear();
    m_device = VK_NULL_HANDLE;
}

void DescriptorCache::ResetFrame(uint32_t frameIndex) {
    if (frameIndex >= m_framePools.size()) {
        return;
    }

    m_currentFrame = frameIndex;
    FramePool& framePool = m_framePools[frameIndex];

    // Reset all pools for this frame
    for (auto pool : framePool.pools) {
        vkResetDescriptorPool(m_device, pool, 0);
    }

    // Start from first pool
    framePool.activePoolIndex = 0;
    framePool.setsAllocatedInActivePool = 0;
}

VkDescriptorSet DescriptorCache::Allocate(VkDescriptorSetLayout layout) {
    VkDescriptorSet set = VK_NULL_HANDLE;
    AllocateBatch(&layout, 1, &set);
    return set;
}

bool DescriptorCache::AllocateBatch(const VkDescriptorSetLayout* layouts, uint32_t count, VkDescriptorSet* outSets) {
    if (count == 0 || layouts == nullptr || outSets == nullptr) {
        return false;
    }

    if (m_currentFrame >= m_framePools.size()) {
        return false;
    }

    VkDescriptorPool pool = GetAvailablePool();
    if (pool == VK_NULL_HANDLE) {
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = count;
    allocInfo.pSetLayouts = layouts;

    VkResult result = vkAllocateDescriptorSets(m_device, &allocInfo, outSets);
    
    if (result == VK_SUCCESS) {
        m_framePools[m_currentFrame].setsAllocatedInActivePool += count;
        return true;
    }

    // Pool exhausted, try next pool
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        FramePool& framePool = m_framePools[m_currentFrame];
        framePool.activePoolIndex++;
        
        pool = GetAvailablePool();
        if (pool == VK_NULL_HANDLE) {
            return false;
        }

        allocInfo.descriptorPool = pool;
        result = vkAllocateDescriptorSets(m_device, &allocInfo, outSets);
        
        if (result == VK_SUCCESS) {
            framePool.setsAllocatedInActivePool = count;
            return true;
        }
    }

    return false;
}

DescriptorCache::Stats DescriptorCache::GetStats() const {
    Stats stats{};
    if (m_currentFrame < m_framePools.size()) {
        const FramePool& framePool = m_framePools[m_currentFrame];
        stats.totalPools = static_cast<uint32_t>(framePool.pools.size());
        stats.activePoolIndex = framePool.activePoolIndex;
        stats.setsAllocated = framePool.setsAllocatedInActivePool;
    }
    return stats;
}

VkDescriptorPool DescriptorCache::CreatePool() {
    std::array<VkDescriptorPoolSize, 4> poolSizes{};
    uint32_t poolSizeCount = 0;

    if (m_config.uniformBufferCount > 0) {
        poolSizes[poolSizeCount].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[poolSizeCount].descriptorCount = m_config.uniformBufferCount;
        poolSizeCount++;
    }

    if (m_config.combinedSamplerCount > 0) {
        poolSizes[poolSizeCount].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[poolSizeCount].descriptorCount = m_config.combinedSamplerCount;
        poolSizeCount++;
    }

    if (m_config.storageBufferCount > 0) {
        poolSizes[poolSizeCount].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[poolSizeCount].descriptorCount = m_config.storageBufferCount;
        poolSizeCount++;
    }

    if (m_config.storageImageCount > 0) {
        poolSizes[poolSizeCount].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[poolSizeCount].descriptorCount = m_config.storageImageCount;
        poolSizeCount++;
    }

    if (poolSizeCount == 0) {
        return VK_NULL_HANDLE;
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = 0; // No individual free, reset entire pool
    poolInfo.maxSets = m_config.maxSets;
    poolInfo.poolSizeCount = poolSizeCount;
    poolInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    return pool;
}

VkDescriptorPool DescriptorCache::GetAvailablePool() {
    if (m_currentFrame >= m_framePools.size()) {
        return VK_NULL_HANDLE;
    }

    FramePool& framePool = m_framePools[m_currentFrame];

    // If we've exhausted current pool, need to move to next or create new
    if (framePool.setsAllocatedInActivePool >= m_config.maxSets) {
        framePool.activePoolIndex++;
    }

    // Create new pool if needed
    while (framePool.activePoolIndex >= framePool.pools.size()) {
        VkDescriptorPool newPool = CreatePool();
        if (newPool == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }
        framePool.pools.push_back(newPool);
    }

    return framePool.pools[framePool.activePoolIndex];
}
