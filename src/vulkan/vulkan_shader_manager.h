#pragma once

#include "thread/job_queue.h"
#include <vulkan/vulkan.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>

/* Deleter for shared_ptr<VkShaderModule>: destroys the module and deletes the allocated handle. */
struct ShaderModuleDeleter {
    VkDevice device = VK_NULL_HANDLE;
    void operator()(VkShaderModule* p) const;
};

/* shared_ptr<VkShaderModule>; construct with (new VkShaderModule(module), ShaderModuleDeleter{device}) so deleter runs when last ref drops. */
using ShaderModulePtr = std::shared_ptr<VkShaderModule>;

/*
 * Shader manager: load SPIR-V via job queue, cache shared_ptr<VkShaderModule> with custom deleter.
 * GetShader/GetShaderIfReady return shared_ptr; when the last ref is dropped the deleter destroys the module.
 * No manual ref-count or Release(). TrimUnused() removes cache entries where only the cache holds a ref.
 */
class VulkanShaderManager {
public:
    VulkanShaderManager() = default;
    ~VulkanShaderManager();

    void Create(JobQueue* pJobQueue_ic);
    void Destroy();

    void RequestLoad(const std::string& sPath);
    bool IsLoadReady(const std::string& sPath);

    /* Non-blocking: return shared_ptr if load completed and module created; else nullptr. */
    ShaderModulePtr GetShaderIfReady(VkDevice device, const std::string& sPath);

    /* Blocking: get or load shader; returns nullptr if load failed. */
    ShaderModulePtr GetShader(VkDevice device, const std::string& sPath);

    void TrimUnused();

    bool IsValid() const { return this->m_pJobQueue != nullptr; }

private:
    VkShaderModule CreateModuleFromSpirv(VkDevice device, const uint8_t* pData, size_t zSize);
    ShaderModulePtr CompletePendingLoad(VkDevice device, const std::string& sPath);

    JobQueue* m_pJobQueue = nullptr;
    std::map<std::string, ShaderModulePtr> m_cache;
    std::map<std::string, std::shared_ptr<LoadFileResult>> m_pending;
    std::mutex m_mutex;
};
