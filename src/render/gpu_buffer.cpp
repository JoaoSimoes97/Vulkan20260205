/**
 * GPUBuffer — Implementation.
 *
 * Phase 4.1: Ring-Buffered GPU Resources
 */

#include "gpu_buffer.h"
#include <cstring>
#include <stdexcept>

GPUBuffer::~GPUBuffer() {
    // Note: Destroy() must be called explicitly before destruction
    // to ensure proper Vulkan resource cleanup order
    if (m_buffer != VK_NULL_HANDLE) {
        // Log warning: buffer not properly destroyed
    }
}

GPUBuffer::GPUBuffer(GPUBuffer&& other) noexcept
    : m_device(other.m_device)
    , m_buffer(other.m_buffer)
    , m_memory(other.m_memory)
    , m_size(other.m_size)
    , m_mappedPtr(other.m_mappedPtr)
    , m_persistent(other.m_persistent)
{
    other.m_device = VK_NULL_HANDLE;
    other.m_buffer = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
    other.m_size = 0;
    other.m_mappedPtr = nullptr;
    other.m_persistent = false;
}

GPUBuffer& GPUBuffer::operator=(GPUBuffer&& other) noexcept {
    if (this != &other) {
        Destroy();

        m_device = other.m_device;
        m_buffer = other.m_buffer;
        m_memory = other.m_memory;
        m_size = other.m_size;
        m_mappedPtr = other.m_mappedPtr;
        m_persistent = other.m_persistent;

        other.m_device = VK_NULL_HANDLE;
        other.m_buffer = VK_NULL_HANDLE;
        other.m_memory = VK_NULL_HANDLE;
        other.m_size = 0;
        other.m_mappedPtr = nullptr;
        other.m_persistent = false;
    }
    return *this;
}

bool GPUBuffer::Create(VkDevice device,
                       VkPhysicalDevice physicalDevice,
                       VkDeviceSize size,
                       VkBufferUsageFlags usage,
                       VkMemoryPropertyFlags properties,
                       bool persistentMap) {
    if (device == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE || size == 0) {
        return false;
    }

    // Clean up any existing resources
    Destroy();

    m_device = device;
    m_size = size;
    m_persistent = persistentMap;

    // Create buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS) {
        return false;
    }

    // Get memory requirements
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, m_buffer, &memRequirements);

    // Find suitable memory type
    uint32_t memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);
    if (memoryTypeIndex == UINT32_MAX) {
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        return false;
    }

    // Allocate memory
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        return false;
    }

    // Bind memory to buffer
    if (vkBindBufferMemory(device, m_buffer, m_memory, 0) != VK_SUCCESS) {
        vkFreeMemory(device, m_memory, nullptr);
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        m_memory = VK_NULL_HANDLE;
        return false;
    }

    // Persistent map if requested (for HOST_VISIBLE memory)
    if (persistentMap && (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        if (vkMapMemory(device, m_memory, 0, size, 0, &m_mappedPtr) != VK_SUCCESS) {
            m_mappedPtr = nullptr;
            // Not fatal - buffer still usable with explicit Map/Unmap
        }
    }

    return true;
}

void GPUBuffer::Destroy() {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }

    // Unmap if persistently mapped
    if (m_mappedPtr != nullptr) {
        vkUnmapMemory(m_device, m_memory);
        m_mappedPtr = nullptr;
    }

    // Destroy buffer
    if (m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }

    // Free memory
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }

    m_device = VK_NULL_HANDLE;
    m_size = 0;
    m_persistent = false;
}

void* GPUBuffer::GetMappedPtr(VkDeviceSize offset) const {
    if (m_mappedPtr == nullptr) {
        return nullptr;
    }
    return static_cast<uint8_t*>(m_mappedPtr) + offset;
}

void* GPUBuffer::Map(VkDeviceSize offset, VkDeviceSize size) {
    if (m_device == VK_NULL_HANDLE || m_memory == VK_NULL_HANDLE) {
        return nullptr;
    }

    // If already mapped (persistent), return existing pointer
    if (m_mappedPtr != nullptr) {
        return static_cast<uint8_t*>(m_mappedPtr) + offset;
    }

    void* ptr = nullptr;
    VkDeviceSize mapSize = (size == VK_WHOLE_SIZE) ? (m_size - offset) : size;

    if (vkMapMemory(m_device, m_memory, offset, mapSize, 0, &ptr) != VK_SUCCESS) {
        return nullptr;
    }

    return ptr;
}

void GPUBuffer::Unmap() {
    // Don't unmap if persistent
    if (m_persistent || m_device == VK_NULL_HANDLE || m_memory == VK_NULL_HANDLE) {
        return;
    }

    vkUnmapMemory(m_device, m_memory);
}

void GPUBuffer::Flush(VkDeviceSize offset, VkDeviceSize size) {
    if (m_device == VK_NULL_HANDLE || m_memory == VK_NULL_HANDLE) {
        return;
    }

    VkMappedMemoryRange memoryRange{};
    memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    memoryRange.memory = m_memory;
    memoryRange.offset = offset;
    memoryRange.size = size;

    vkFlushMappedMemoryRanges(m_device, 1, &memoryRange);
}

void GPUBuffer::Invalidate(VkDeviceSize offset, VkDeviceSize size) {
    if (m_device == VK_NULL_HANDLE || m_memory == VK_NULL_HANDLE) {
        return;
    }

    VkMappedMemoryRange memoryRange{};
    memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    memoryRange.memory = m_memory;
    memoryRange.offset = offset;
    memoryRange.size = size;

    vkInvalidateMappedMemoryRanges(m_device, 1, &memoryRange);
}

uint32_t GPUBuffer::FindMemoryType(VkPhysicalDevice physicalDevice,
                                    uint32_t typeFilter,
                                    VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return UINT32_MAX;
}
