/*
 * MeshManager — procedural meshes with vertex buffers; async .obj load and upload.
 */
#include "mesh_manager.h"
#include "thread/job_queue.h"
#include "vulkan/vulkan_utils.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace {
    uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((typeFilter & (1u << i)) &&
                (memProps.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }
        throw std::runtime_error("MeshManager: no suitable memory type");
    }
}

// -----------------------------------------------------------------------------
// MeshHandle
// -----------------------------------------------------------------------------
MeshHandle::~MeshHandle() {
    Destroy();
}

MeshHandle::MeshHandle(MeshHandle&& other) noexcept
    : m_device(other.m_device)
    , m_vertexBuffer(other.m_vertexBuffer)
    , m_vertexBufferMemory(other.m_vertexBufferMemory)
    , m_vertexCount(other.m_vertexCount)
    , m_instanceCount(other.m_instanceCount)
    , m_firstVertex(other.m_firstVertex)
    , m_firstInstance(other.m_firstInstance) {
    other.m_device = VK_NULL_HANDLE;
    other.m_vertexBuffer = VK_NULL_HANDLE;
    other.m_vertexBufferMemory = VK_NULL_HANDLE;
    other.m_vertexCount = 0u;
}

MeshHandle& MeshHandle::operator=(MeshHandle&& other) noexcept {
    if (this == &other) return *this;
    Destroy();
    m_device = other.m_device;
    m_vertexBuffer = other.m_vertexBuffer;
    m_vertexBufferMemory = other.m_vertexBufferMemory;
    m_vertexCount = other.m_vertexCount;
    m_instanceCount = other.m_instanceCount;
    m_firstVertex = other.m_firstVertex;
    m_firstInstance = other.m_firstInstance;
    other.m_device = VK_NULL_HANDLE;
    other.m_vertexBuffer = VK_NULL_HANDLE;
    other.m_vertexBufferMemory = VK_NULL_HANDLE;
    other.m_vertexCount = 0u;
    return *this;
}

void MeshHandle::SetVertexBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory memory) {
    Destroy();
    m_device = device;
    m_vertexBuffer = buffer;
    m_vertexBufferMemory = memory;
}

void MeshHandle::SetDrawParams(uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance) {
    m_vertexCount = vertexCount;
    m_firstVertex = firstVertex;
    m_instanceCount = instanceCount;
    m_firstInstance = firstInstance;
}

void MeshHandle::Destroy() {
    if (m_device != VK_NULL_HANDLE && m_vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
    }
    if (m_device != VK_NULL_HANDLE && m_vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
        m_vertexBufferMemory = VK_NULL_HANDLE;
    }
    m_device = VK_NULL_HANDLE;
    m_vertexCount = 0u;
}

// -----------------------------------------------------------------------------
// MeshManager
// -----------------------------------------------------------------------------
void MeshManager::SetJobQueue(JobQueue* pJobQueue) {
    m_pJobQueue = pJobQueue;
}

void MeshManager::SetDevice(VkDevice device) {
    m_device = device;
}

void MeshManager::SetPhysicalDevice(VkPhysicalDevice physicalDevice) {
    m_physicalDevice = physicalDevice;
}

void MeshManager::SetQueue(VkQueue queue) {
    m_queue = queue;
}

void MeshManager::SetQueueFamilyIndex(uint32_t queueFamilyIndex) {
    m_queueFamilyIndex = queueFamilyIndex;
}

std::shared_ptr<MeshHandle> MeshManager::CreateVertexBufferFromData(const void* pData, uint32_t vertexCount, uint32_t vertexStride) {
    if (m_device == VK_NULL_HANDLE || m_physicalDevice == VK_NULL_HANDLE || m_queue == VK_NULL_HANDLE ||
        pData == nullptr || vertexCount == 0u || vertexStride == 0u)
        return nullptr;
    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(vertexStride) * vertexCount;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    {
        VkBufferCreateInfo bufInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = static_cast<VkBufferCreateFlags>(0),
            .size = bufferSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
        };
        VkResult r = vkCreateBuffer(m_device, &bufInfo, nullptr, &stagingBuffer);
        if (r != VK_SUCCESS) return nullptr;
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memReqs);
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = FindMemoryType(m_physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        };
        r = vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingMemory);
        if (r != VK_SUCCESS) {
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            return nullptr;
        }
        vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0);
        void* pMapped = nullptr;
        vkMapMemory(m_device, stagingMemory, 0, bufferSize, 0, &pMapped);
        if (pMapped) {
            std::memcpy(pMapped, pData, static_cast<size_t>(bufferSize));
            vkUnmapMemory(m_device, stagingMemory);
        }
    }

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    {
        VkBufferCreateInfo bufInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = static_cast<VkBufferCreateFlags>(0),
            .size = bufferSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
        };
        VkResult r = vkCreateBuffer(m_device, &bufInfo, nullptr, &vertexBuffer);
        if (r != VK_SUCCESS) {
            vkFreeMemory(m_device, stagingMemory, nullptr);
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            return nullptr;
        }
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(m_device, vertexBuffer, &memReqs);
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = FindMemoryType(m_physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        };
        r = vkAllocateMemory(m_device, &allocInfo, nullptr, &vertexMemory);
        if (r != VK_SUCCESS) {
            vkDestroyBuffer(m_device, vertexBuffer, nullptr);
            vkFreeMemory(m_device, stagingMemory, nullptr);
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            return nullptr;
        }
        vkBindBufferMemory(m_device, vertexBuffer, vertexMemory, 0);
    }

    VkCommandPool cmdPool = VK_NULL_HANDLE;
    VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
    {
        VkCommandPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = m_queueFamilyIndex,
        };
        VkResult r = vkCreateCommandPool(m_device, &poolInfo, nullptr, &cmdPool);
        if (r != VK_SUCCESS) {
            vkFreeMemory(m_device, vertexMemory, nullptr);
            vkDestroyBuffer(m_device, vertexBuffer, nullptr);
            vkFreeMemory(m_device, stagingMemory, nullptr);
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            return nullptr;
        }
        VkCommandBufferAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = cmdPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        r = vkAllocateCommandBuffers(m_device, &allocInfo, &cmdBuf);
        if (r != VK_SUCCESS) {
            vkDestroyCommandPool(m_device, cmdPool, nullptr);
            vkFreeMemory(m_device, vertexMemory, nullptr);
            vkDestroyBuffer(m_device, vertexBuffer, nullptr);
            vkFreeMemory(m_device, stagingMemory, nullptr);
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            return nullptr;
        }
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdBuf, &beginInfo);
        VkBufferCopy copy = { .srcOffset = 0, .dstOffset = 0, .size = bufferSize };
        vkCmdCopyBuffer(cmdBuf, stagingBuffer, vertexBuffer, 1, &copy);
        vkEndCommandBuffer(cmdBuf);
    }

    VkFence fence = VK_NULL_HANDLE;
    {
        VkFenceCreateInfo fenceInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = static_cast<VkFenceCreateFlags>(0),
        };
        VkResult r = vkCreateFence(m_device, &fenceInfo, nullptr, &fence);
        if (r != VK_SUCCESS) {
            vkDestroyCommandPool(m_device, cmdPool, nullptr);
            vkFreeMemory(m_device, vertexMemory, nullptr);
            vkDestroyBuffer(m_device, vertexBuffer, nullptr);
            vkFreeMemory(m_device, stagingMemory, nullptr);
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            return nullptr;
        }
        VkSubmitInfo submit = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmdBuf,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr,
        };
        vkQueueSubmit(m_queue, 1, &submit, fence);
        vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(m_device, fence, nullptr);
    }

    vkDestroyCommandPool(m_device, cmdPool, nullptr);
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);

    auto handle = std::make_shared<MeshHandle>();
    handle->SetVertexBuffer(m_device, vertexBuffer, vertexMemory);
    handle->SetDrawParams(vertexCount, 0u, 1u, 0u);
    return handle;
}

namespace {
    void TrianglePositions(std::vector<float>& out) {
        out = { 0.f, -0.5f, 0.f,  0.5f, 0.5f, 0.f,  -0.5f, 0.5f, 0.f };
    }
    void RectanglePositions(std::vector<float>& out) {
        out = {
            -0.5f, -0.5f, 0.f,  0.5f, -0.5f, 0.f,  0.5f, 0.5f, 0.f,
            -0.5f, -0.5f, 0.f,  0.5f, 0.5f, 0.f,  -0.5f, 0.5f, 0.f
        };
    }
    void CubePositions(std::vector<float>& out) {
        float s = 0.5f;
        out = {
            -s,-s,-s, s,-s,-s, s,s,-s,  -s,-s,-s, s,s,-s, -s,s,-s,
            -s,-s, s, s,s, s, s,-s, s,  -s,-s, s, -s,s, s, s,s, s,
            -s,-s,-s, -s,s,-s, -s,s, s,  -s,-s,-s, -s,s, s, -s,-s, s,
            s,-s,-s, s,-s, s, s,s, s,   s,-s,-s, s,s, s, s,s,-s,
            -s,-s, s, s,-s, s, s,-s,-s,  -s,-s, s, s,-s,-s, -s,-s,-s,
            -s, s, s, s, s,-s, s, s, s,  -s, s, s, -s, s,-s, s, s,-s
        };
    }
}

std::shared_ptr<MeshHandle> MeshManager::GetOrCreateFromPositions(const std::string& key, const float* pPositions, uint32_t vertexCount) {
    if (key.empty() || pPositions == nullptr || vertexCount == 0u)
        return nullptr;
    auto it = m_cache.find(key);
    if (it != m_cache.end())
        return it->second;
    // Legacy: position-only data (3 floats per vertex)
    std::shared_ptr<MeshHandle> p = CreateVertexBufferFromData(pPositions, vertexCount, sizeof(float) * 3u);
    if (p)
        m_cache[key] = p;
    return p;
}

std::shared_ptr<MeshHandle> MeshManager::GetOrCreateFromGltf(const std::string& key, const void* pVertexData, uint32_t vertexCount) {
    if (key.empty() || pVertexData == nullptr || vertexCount == 0u)
        return nullptr;
    auto it = m_cache.find(key);
    if (it != m_cache.end())
        return it->second;
    // glTF meshes use interleaved vertex data (pos+UV+normal, 32 bytes per vertex)
    constexpr uint32_t vertexStride = 32u; // sizeof(VertexData) = 8 floats * 4 bytes
    std::shared_ptr<MeshHandle> p = CreateVertexBufferFromData(pVertexData, vertexCount, vertexStride);
    if (p)
        m_cache[key] = p;
    return p;
}

std::shared_ptr<MeshHandle> MeshManager::GetOrCreateProcedural(const std::string& key) {
    auto it = m_cache.find(key);
    if (it != m_cache.end())
        return it->second;
    std::vector<float> positions;
    if (key == "triangle")
        TrianglePositions(positions);
    else if (key == "circle" || key == "rectangle")
        RectanglePositions(positions);
    else if (key == "cube")
        CubePositions(positions);
    else
        TrianglePositions(positions);
    const uint32_t vertexCount = static_cast<uint32_t>(positions.size() / 3);
    // Procedural meshes are position-only (3 floats per vertex)
    std::shared_ptr<MeshHandle> p = CreateVertexBufferFromData(positions.data(), vertexCount, sizeof(float) * 3u);
    if (p)
        m_cache[key] = p;
    return p;
}

void MeshManager::RequestLoadMesh(const std::string& path) {
    if (m_pJobQueue == nullptr) return;
    if (m_pendingMeshPaths.count(path) != 0) return;
    if (m_cache.count(path) != 0) return;
    m_pendingMeshPaths.insert(path);
    m_pJobQueue->SubmitLoadFile(path);
}

void MeshManager::OnCompletedMeshFile(const std::string& sPath_ic, std::vector<uint8_t> vecData_in) {
    if (this->m_pendingMeshPaths.erase(sPath_ic) == 0)
        return;
    std::vector<float> vecPositions;
    uint32_t lVertexCount = static_cast<uint32_t>(0u);
    if ((ParseObj(vecData_in.data(), vecData_in.size(), vecPositions, lVertexCount) == false) || (lVertexCount == 0u)) {
        VulkanUtils::LogErr("MeshManager: failed to parse {}", sPath_ic);
        return;
    }
    // OBJ files are position-only (3 floats per vertex)
    std::shared_ptr<MeshHandle> pHandle = CreateVertexBufferFromData(vecPositions.data(), lVertexCount, sizeof(float) * 3u);
    if (pHandle != nullptr) {
        this->m_cache[sPath_ic] = pHandle;
        VulkanUtils::LogInfo("MeshManager: loaded {} ({} verts)", sPath_ic, lVertexCount);
    }
}

bool MeshManager::ParseObj(const uint8_t* pData, size_t size, std::vector<float>& outPositions, uint32_t& outVertexCount) {
    outPositions.clear();
    outVertexCount = 0u;
    if (pData == nullptr) return false;
    std::vector<float> verts;
    const char* p = reinterpret_cast<const char*>(pData);
    const char* end = p + size;
    while (p < end) {
        if (p + 2 <= end && p[0] == 'v' && (p[1] == ' ' || p[1] == '\t')) {
            p += 2;
            float x = 0.f, y = 0.f, z = 0.f;
            if (p < end) { x = static_cast<float>(std::strtod(p, const_cast<char**>(&p))); }
            if (p < end) { y = static_cast<float>(std::strtod(p, const_cast<char**>(&p))); }
            if (p < end) { z = static_cast<float>(std::strtod(p, const_cast<char**>(&p))); }
            verts.push_back(x);
            verts.push_back(y);
            verts.push_back(z);
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
            continue;
        }
        if (p + 2 <= end && p[0] == 'f' && (p[1] == ' ' || p[1] == '\t')) {
            p += 2;
            std::vector<uint32_t> indices;
            while (p < end && *p != '\n') {
                while (p < end && (*p == ' ' || *p == '\t')) p++;
                if (p >= end || *p == '\n') break;
                unsigned long idx = 0;
                if (*p >= '0' && *p <= '9')
                    idx = std::strtoul(p, const_cast<char**>(&p), 10);
                if (idx >= 1u && idx <= verts.size() / 3u)
                    indices.push_back(static_cast<uint32_t>(idx - 1u));
                while (p < end && *p != ' ' && *p != '\t' && *p != '\n') p++;
            }
            for (size_t i = 2; i < indices.size(); ++i) {
                uint32_t a = indices[0], b = indices[static_cast<size_t>(i - 1)], c = indices[i];
                for (int j = 0; j < 3; ++j) outPositions.push_back(verts[a * 3 + j]);
                for (int j = 0; j < 3; ++j) outPositions.push_back(verts[b * 3 + j]);
                for (int j = 0; j < 3; ++j) outPositions.push_back(verts[c * 3 + j]);
            }
            if (p < end) p++;
            continue;
        }
        while (p < end && *p != '\n') p++;
        if (p < end) p++;
    }
    outVertexCount = static_cast<uint32_t>(outPositions.size() / 3);
    if (outVertexCount == 0u && !verts.empty())
        outPositions = verts, outVertexCount = static_cast<uint32_t>(verts.size() / 3);
    return outVertexCount > 0u;
}

std::shared_ptr<MeshHandle> MeshManager::GetMesh(const std::string& key) const {
    auto it = m_cache.find(key);
    if (it == m_cache.end())
        return nullptr;
    return it->second;
}

void MeshManager::TrimUnused() {
    for (auto it = m_cache.begin(); it != m_cache.end(); ) {
        if (it->second.use_count() == 1u) {
            m_pendingDestroy.push_back(std::move(it->second));
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void MeshManager::ProcessPendingDestroys() {
    m_pendingDestroy.clear();  /* shared_ptrs released → MeshHandle destructors → vkDestroyBuffer; safe after vkWaitForFences */
}

void MeshManager::Destroy() {
    m_pendingMeshPaths.clear();
    m_pendingDestroy.clear();
    m_cache.clear();
}
