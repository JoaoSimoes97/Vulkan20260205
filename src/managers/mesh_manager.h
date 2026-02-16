#pragma once

#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

struct JobQueue;

/**
 * Mesh handle: owns vertex buffer (and optionally index buffer later). Destructor frees GPU resources.
 * Draw params: vertexCount, firstVertex, instanceCount, firstInstance; indexCount/firstIndex for future indexed draw.
 */
class MeshHandle {
public:
    MeshHandle() = default;
    ~MeshHandle();

    MeshHandle(const MeshHandle&) = delete;
    MeshHandle& operator=(const MeshHandle&) = delete;
    MeshHandle(MeshHandle&& other) noexcept;
    MeshHandle& operator=(MeshHandle&& other) noexcept;

    void SetVertexBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory memory);
    void SetDrawParams(uint32_t vertexCount, uint32_t firstVertex = 0u, uint32_t instanceCount = 1u, uint32_t firstInstance = 0u);

    VkBuffer GetVertexBuffer() const { return m_vertexBuffer; }
    VkDeviceSize GetVertexBufferOffset() const { return 0; }
    uint32_t GetVertexCount() const { return m_vertexCount; }
    uint32_t GetInstanceCount() const { return m_instanceCount; }
    uint32_t GetFirstVertex() const { return m_firstVertex; }
    uint32_t GetFirstInstance() const { return m_firstInstance; }
    bool HasValidBuffer() const { return m_vertexBuffer != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE; }

private:
    void Destroy();

    VkDevice m_device = VK_NULL_HANDLE;
    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    uint32_t m_vertexCount   = 0u;
    uint32_t m_instanceCount = 1u;
    uint32_t m_firstVertex   = 0u;
    uint32_t m_firstInstance = 0u;
};

/**
 * Get-or-create procedural meshes (with vertex buffers); load mesh files async via RequestLoadMesh.
 * SetDevice/SetPhysicalDevice/SetQueue/SetQueueFamilyIndex before GetOrCreateProcedural or file meshes.
 * Destroy() clears cache (call before device destroy).
 */
class MeshManager {
public:
    MeshManager() = default;

    void SetJobQueue(JobQueue* pJobQueue);
    void SetDevice(VkDevice device);
    void SetPhysicalDevice(VkPhysicalDevice physicalDevice);
    void SetQueue(VkQueue queue);
    void SetQueueFamilyIndex(uint32_t queueFamilyIndex);

    std::shared_ptr<MeshHandle> GetOrCreateProcedural(const std::string& key);
    /** Create mesh from position data; cache by key (e.g. gltfPath + ":" + meshIndex). */
    std::shared_ptr<MeshHandle> GetOrCreateFromPositions(const std::string& key, const float* pPositions, uint32_t vertexCount);
    /** Create mesh from glTF (interleaved pos+UV+normal); cache by key (e.g. gltfPath + ":" + meshIndex + ":" + primitiveIndex). */
    std::shared_ptr<MeshHandle> GetOrCreateFromGltf(const std::string& key, const void* pVertexData, uint32_t vertexCount);
    void RequestLoadMesh(const std::string& path);
    void OnCompletedMeshFile(const std::string& sPath_ic, std::vector<uint8_t> vecData_in);

    std::shared_ptr<MeshHandle> GetMesh(const std::string& key) const;
    void TrimUnused();
    /** Destroy mesh buffers that were trimmed. Call at start of frame after vkWaitForFences (buffers may still be in use until then). */
    void ProcessPendingDestroys();
    /** Clear all cached meshes (release buffers). Call before device destroy. */
    void Destroy();

private:
    std::shared_ptr<MeshHandle> CreateVertexBufferFromData(const void* pData, uint32_t vertexCount, uint32_t vertexStride);
    bool ParseObj(const uint8_t* pData, size_t size, std::vector<float>& outPositions, uint32_t& outVertexCount);

    JobQueue* m_pJobQueue = nullptr;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    uint32_t m_queueFamilyIndex = 0u;
    std::map<std::string, std::shared_ptr<MeshHandle>> m_cache;
    std::set<std::string> m_pendingMeshPaths;
    /** Meshes trimmed from cache; destroyed in ProcessPendingDestroys() after fence wait. */
    std::vector<std::shared_ptr<MeshHandle>> m_pendingDestroy;
};
