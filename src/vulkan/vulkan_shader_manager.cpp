#include "vulkan_shader_manager.h"
#include "vulkan_utils.h"
#include <cstring>
#include <stdexcept>

void ShaderModuleDeleter::operator()(VkShaderModule* p) const {
    if (p && *p != VK_NULL_HANDLE && device != VK_NULL_HANDLE)
        vkDestroyShaderModule(device, *p, nullptr);
    delete p;
}

void VulkanShaderManager::Create(JobQueue* pJobQueue) {
    VulkanUtils::LogTrace("VulkanShaderManager::Create");
    if (pJobQueue == nullptr) {
        VulkanUtils::LogErr("VulkanShaderManager::Create: null JobQueue");
        throw std::runtime_error("VulkanShaderManager::Create: null JobQueue");
    }
    this->m_pJobQueue = pJobQueue;
}

void VulkanShaderManager::Destroy() {
    VulkanUtils::LogTrace("VulkanShaderManager::Destroy");
    this->m_cache.clear();
    this->m_pending.clear();
    this->m_pJobQueue = nullptr;
}

VkShaderModule VulkanShaderManager::CreateModuleFromSpirv(VkDevice device, const uint8_t* pData, size_t zSize) {
    if ((pData == nullptr) || (zSize == static_cast<size_t>(0)) || ((zSize % sizeof(uint32_t)) != static_cast<size_t>(0))) {
        VulkanUtils::LogErr("Invalid SPIR-V data for CreateModuleFromSpirv");
        return VK_NULL_HANDLE;
    }
    VkShaderModuleCreateInfo stInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = static_cast<VkShaderModuleCreateFlags>(0),
        .codeSize = zSize,
        .pCode    = reinterpret_cast<const uint32_t*>(pData),
    };
    VkShaderModule module = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(device, &stInfo, nullptr, &module);
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateShaderModule failed: {}", static_cast<int>(result));
        return VK_NULL_HANDLE;
    }
    return module;
}

void VulkanShaderManager::RequestLoad(const std::string& sPath) {
    if (this->m_pJobQueue == nullptr)
        return;
    std::lock_guard<std::mutex> lock(this->m_mutex);
    if (this->m_cache.find(sPath) != this->m_cache.end())
        return;
    if (this->m_pending.find(sPath) != this->m_pending.end())
        return;
    std::shared_ptr<LoadFileResult> pResult = this->m_pJobQueue->SubmitLoadFile(sPath);
    this->m_pending[sPath] = pResult;
}

bool VulkanShaderManager::IsLoadReady(const std::string& sPath) {
    if (this->m_pJobQueue == nullptr)
        return false;
    std::lock_guard<std::mutex> lock(this->m_mutex);
    if (this->m_cache.find(sPath) != this->m_cache.end())
        return true;
    auto it = this->m_pending.find(sPath);
    if (it == this->m_pending.end())
        return false;
    std::lock_guard<std::mutex> rlock(it->second->mtx);
    return it->second->bDone;
}

ShaderModulePtr VulkanShaderManager::CompletePendingLoad(VkDevice device, const std::string& sPath) {
    auto itPending = this->m_pending.find(sPath);
    if (itPending == this->m_pending.end())
        return nullptr;
    std::shared_ptr<LoadFileResult> pResult = itPending->second;
    std::unique_lock<std::mutex> rlock(pResult->mtx);
    if (pResult->bDone == false)
        return nullptr;
    if (pResult->vecData.empty() == true) {
        VulkanUtils::LogErr("Shader file not found or empty: {}", sPath);
        this->m_pending.erase(itPending);
        return nullptr;
    }
    std::vector<uint8_t> vecCopy = pResult->vecData;
    rlock.unlock();
    this->m_pending.erase(itPending);

    VkShaderModule module = this->CreateModuleFromSpirv(device, vecCopy.data(), vecCopy.size());
    if (module == VK_NULL_HANDLE)
        return nullptr;

    ShaderModulePtr ptr(new VkShaderModule(module), ShaderModuleDeleter{device});
    this->m_cache[sPath] = ptr;
    return ptr;
}

ShaderModulePtr VulkanShaderManager::GetShaderIfReady(VkDevice device, const std::string& sPath) {
    if (this->m_pJobQueue == nullptr || device == VK_NULL_HANDLE)
        return nullptr;
    std::lock_guard<std::mutex> lock(this->m_mutex);
    auto itCache = this->m_cache.find(sPath);
    if (itCache != this->m_cache.end())
        return itCache->second;
    return this->CompletePendingLoad(device, sPath);
}

ShaderModulePtr VulkanShaderManager::GetShader(VkDevice device, const std::string& sPath) {
    if (this->m_pJobQueue == nullptr) {
        VulkanUtils::LogErr("VulkanShaderManager::GetShader: not created");
        return nullptr;
    }
    if (device == VK_NULL_HANDLE)
        return nullptr;

    std::lock_guard<std::mutex> lock(this->m_mutex);
    auto it = this->m_cache.find(sPath);
    if (it != this->m_cache.end())
        return it->second;

    auto itPending = this->m_pending.find(sPath);
    if (itPending == this->m_pending.end()) {
        std::shared_ptr<LoadFileResult> pResult = this->m_pJobQueue->SubmitLoadFile(sPath);
        this->m_pending[sPath] = pResult;
        itPending = this->m_pending.find(sPath);
    }
    std::shared_ptr<LoadFileResult> pResult = itPending->second;
    {
        std::unique_lock<std::mutex> rlock(pResult->mtx);
        while (pResult->bDone == false)
            pResult->cv.wait(rlock);
    }

    return this->CompletePendingLoad(device, sPath);
}

void VulkanShaderManager::TrimUnused() {
    std::lock_guard<std::mutex> lock(this->m_mutex);
    for (auto it = this->m_cache.begin(); it != this->m_cache.end(); ) {
        if (it->second.use_count() == 1u)
            it = this->m_cache.erase(it);
        else
            ++it;
    }
}

VulkanShaderManager::~VulkanShaderManager() {
    this->Destroy();
}
