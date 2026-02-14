#pragma once

#include "thread/job_queue.h"
#include <vulkan/vulkan.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>

/*
 * Shader manager: load SPIR-V via job queue, cache VkShaderModule, ref-count. When ref 0 the module is destroyed and removed from cache so shaders can be unloaded when no pipeline uses them (e.g. on DestroyPipelines or when streaming a huge world).
 * Pipelines hold a ref (VulkanPipeline::Create does GetShader and does not Release on success; Destroy releases). Multiple pipelines can share the same shader; when all are destroyed the shader is unloaded.
 * RequestLoad(path): submits load without blocking. GetShaderIfReady/GetShader return module; Release(path) decrements ref.
 */
class VulkanShaderManager {
public:
    VulkanShaderManager() = default;
    ~VulkanShaderManager();

    void Create(JobQueue* pJobQueue);
    void Destroy();

    /* Submit a load for path without blocking. Idempotent if already requested or cached. */
    void RequestLoad(const std::string& sPath);

    /* True if path is in cache or has a completed load in pending (ready to create module). Use to avoid re-requesting or taking a ref then releasing. */
    bool IsLoadReady(const std::string& sPath);

    /* Non-blocking: return module if load completed and module created; else VK_NULL_HANDLE. */
    VkShaderModule GetShaderIfReady(VkDevice device, const std::string& sPath);

    /* Blocking: get or load shader; creates VkShaderModule on calling thread. Returns VK_NULL_HANDLE if load failed. */
    VkShaderModule GetShader(VkDevice device, const std::string& sPath);

    void Release(const std::string& sPath);

    bool IsValid() const { return this->m_pJobQueue != nullptr; }

private:
    struct CachedShader {
        VkDevice       device = VK_NULL_HANDLE;
        VkShaderModule module = VK_NULL_HANDLE;
        uint32_t       lRefCount = 0;
    };

    VkShaderModule CreateModuleFromSpirv(VkDevice device, const uint8_t* pData, size_t zSize);
    void DestroyCached(CachedShader& stCached);
    /* If path has a completed load in m_pending, create module, cache it, remove from pending, return module. Caller holds m_mutex. */
    VkShaderModule CompletePendingLoad(VkDevice device, const std::string& sPath);

    JobQueue* m_pJobQueue = nullptr;
    std::map<std::string, CachedShader> m_cache;
    std::map<std::string, std::shared_ptr<LoadFileResult>> m_pending;
    std::mutex m_mutex;
};
