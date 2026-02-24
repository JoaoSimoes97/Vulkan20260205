/**
 * GPUBuffer — Vulkan buffer with optional persistent mapping.
 *
 * Supports:
 * - Standard create/destroy lifecycle
 * - Persistent mapping (map once at creation, unmap at destruction)
 * - Ring buffer mode for per-frame data isolation
 *
 * Phase 4.1: Ring-Buffered GPU Resources
 */

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <cstddef>

/**
 * GPUBuffer — Owns a VkBuffer + VkDeviceMemory with optional persistent mapping.
 *
 * For ring-buffered usage:
 *   - Create with totalSize = singleFrameSize * framesInFlight
 *   - Use GetMappedPtr(frameIndex * singleFrameSize) for each frame's region
 *   - No vkMapMemory/vkUnmapMemory calls during rendering
 *
 * Memory selection:
 *   - HOST_VISIBLE | HOST_COHERENT: For CPU-written data (SSBOs, dynamic UBOs)
 *   - DEVICE_LOCAL: For GPU-only data (via staging buffer)
 */
class GPUBuffer {
public:
    GPUBuffer() = default;
    ~GPUBuffer();

    // Non-copyable (owns Vulkan resources)
    GPUBuffer(const GPUBuffer&) = delete;
    GPUBuffer& operator=(const GPUBuffer&) = delete;

    // Movable
    GPUBuffer(GPUBuffer&& other) noexcept;
    GPUBuffer& operator=(GPUBuffer&& other) noexcept;

    /**
     * Create buffer with optional persistent mapping.
     *
     * @param device Vulkan device
     * @param physicalDevice Physical device (for memory type selection)
     * @param size Buffer size in bytes
     * @param usage VkBufferUsageFlags (e.g., VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
     * @param properties VkMemoryPropertyFlags (e.g., VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
     * @param persistentMap If true, map the buffer at creation (for HOST_VISIBLE memory)
     * @return true on success
     */
    bool Create(VkDevice device,
                VkPhysicalDevice physicalDevice,
                VkDeviceSize size,
                VkBufferUsageFlags usage,
                VkMemoryPropertyFlags properties,
                bool persistentMap = false);

    /**
     * Destroy buffer and free memory.
     */
    void Destroy();

    /**
     * Get mapped pointer (only valid if persistentMap was true, or after explicit Map()).
     * @param offset Byte offset into mapped region (default 0)
     * @return Pointer to mapped memory, or nullptr if not mapped
     */
    void* GetMappedPtr(VkDeviceSize offset = 0) const;

    /**
     * Map buffer memory (for non-persistent mapping scenario).
     * @param offset Start offset
     * @param size Size to map (VK_WHOLE_SIZE for entire buffer)
     * @return Mapped pointer, or nullptr on failure
     */
    void* Map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

    /**
     * Unmap buffer memory (for non-persistent mapping scenario).
     */
    void Unmap();

    /**
     * Flush mapped memory range (for non-coherent memory).
     * @param offset Start offset relative to buffer start
     * @param size Size to flush (VK_WHOLE_SIZE for entire buffer)
     */
    void Flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

    /**
     * Invalidate mapped memory range (for non-coherent memory, after GPU write).
     * @param offset Start offset
     * @param size Size to invalidate
     */
    void Invalidate(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

    // Accessors
    VkBuffer GetBuffer() const { return m_buffer; }
    VkDeviceMemory GetMemory() const { return m_memory; }
    VkDeviceSize GetSize() const { return m_size; }
    bool IsMapped() const { return m_mappedPtr != nullptr; }
    bool IsValid() const { return m_buffer != VK_NULL_HANDLE; }

private:
    static uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,
                                   uint32_t typeFilter,
                                   VkMemoryPropertyFlags properties);

    VkDevice        m_device      = VK_NULL_HANDLE;
    VkBuffer        m_buffer      = VK_NULL_HANDLE;
    VkDeviceMemory  m_memory      = VK_NULL_HANDLE;
    VkDeviceSize    m_size        = 0;
    void*           m_mappedPtr   = nullptr;
    bool            m_persistent  = false;
};

/**
 * RingBuffer — Triple-buffered GPU buffer for per-frame data.
 *
 * Automatically manages frame-indexed regions within a single large buffer.
 * Each frame writes to its own region, avoiding CPU/GPU synchronization issues.
 *
 * Template parameter T: Per-element data type (e.g., ObjectData, LightData)
 */
template<typename T>
class RingBuffer {
public:
    RingBuffer() = default;
    ~RingBuffer() = default;

    /**
     * Create ring buffer.
     *
     * @param device Vulkan device
     * @param physicalDevice Physical device
     * @param elementsPerFrame Maximum elements written per frame
     * @param framesInFlight Number of frames to buffer (typically 2 or 3)
     * @param usage VkBufferUsageFlags
     * @return true on success
     */
    bool Create(VkDevice device,
                VkPhysicalDevice physicalDevice,
                uint32_t elementsPerFrame,
                uint32_t framesInFlight,
                VkBufferUsageFlags usage) {
        m_elementsPerFrame = elementsPerFrame;
        m_framesInFlight = framesInFlight;
        m_elementSize = sizeof(T);
        m_frameSize = static_cast<VkDeviceSize>(elementsPerFrame) * m_elementSize;
        m_totalSize = m_frameSize * framesInFlight;

        return m_buffer.Create(device, physicalDevice, m_totalSize, usage,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               true /* persistentMap */);
    }

    /**
     * Destroy ring buffer.
     */
    void Destroy() {
        m_buffer.Destroy();
        m_elementsPerFrame = 0;
        m_framesInFlight = 0;
    }

    /**
     * Get pointer to the start of a frame's region.
     * @param frameIndex Frame index (0 to framesInFlight-1)
     * @return Pointer to T array for this frame
     */
    T* GetFrameData(uint32_t frameIndex) {
        if (!m_buffer.IsMapped() || frameIndex >= m_framesInFlight) {
            return nullptr;
        }
        VkDeviceSize offset = static_cast<VkDeviceSize>(frameIndex) * m_frameSize;
        return static_cast<T*>(m_buffer.GetMappedPtr(offset));
    }

    /**
     * Get byte offset for a frame's region (for descriptor set dynamic offset).
     * @param frameIndex Frame index
     * @return Byte offset into the buffer
     */
    VkDeviceSize GetFrameOffset(uint32_t frameIndex) const {
        return static_cast<VkDeviceSize>(frameIndex) * m_frameSize;
    }

    /**
     * Get byte offset for a specific element within a frame.
     * @param frameIndex Frame index
     * @param elementIndex Element index within the frame
     * @return Byte offset into the buffer
     */
    VkDeviceSize GetElementOffset(uint32_t frameIndex, uint32_t elementIndex) const {
        return GetFrameOffset(frameIndex) + static_cast<VkDeviceSize>(elementIndex) * m_elementSize;
    }

    // Accessors
    VkBuffer GetBuffer() const { return m_buffer.GetBuffer(); }
    VkDeviceSize GetTotalSize() const { return m_totalSize; }
    VkDeviceSize GetFrameSize() const { return m_frameSize; }
    uint32_t GetElementsPerFrame() const { return m_elementsPerFrame; }
    uint32_t GetFramesInFlight() const { return m_framesInFlight; }
    bool IsValid() const { return m_buffer.IsValid(); }

private:
    GPUBuffer       m_buffer;
    uint32_t        m_elementsPerFrame = 0;
    uint32_t        m_framesInFlight   = 0;
    VkDeviceSize    m_elementSize      = 0;
    VkDeviceSize    m_frameSize        = 0;
    VkDeviceSize    m_totalSize        = 0;
};
