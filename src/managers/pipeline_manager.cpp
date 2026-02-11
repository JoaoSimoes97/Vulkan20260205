#include "pipeline_manager.h"

void PipelineManager::RequestPipeline(const std::string& sKey,
                                       VulkanShaderManager* pShaderManager,
                                       const std::string& sVertPath,
                                       const std::string& sFragPath) {
    if (pShaderManager == nullptr || pShaderManager->IsValid() == false)
        return;
    auto it = this->m_entries.find(sKey);
    if (it != this->m_entries.end())
        return;
    this->m_entries[sKey].sVertPath = sVertPath;
    this->m_entries[sKey].sFragPath = sFragPath;
    pShaderManager->RequestLoad(sVertPath);
    pShaderManager->RequestLoad(sFragPath);
}

VkPipeline PipelineManager::GetPipelineIfReady(const std::string& sKey,
                                               VkDevice device,
                                               VkExtent2D extent,
                                               VkRenderPass renderPass,
                                               VulkanShaderManager* pShaderManager,
                                               const GraphicsPipelineParams& pipelineParams) {
    if (device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || pShaderManager == nullptr)
        return VK_NULL_HANDLE;
    auto it = this->m_entries.find(sKey);
    if (it == this->m_entries.end())
        return VK_NULL_HANDLE;

    PipelineEntry& entry = it->second;
    /* Request only paths that are not yet ready (cache or completed pending); avoid re-requesting or releasing the one that finished. */
    if (pShaderManager->IsLoadReady(entry.sVertPath) == false)
        pShaderManager->RequestLoad(entry.sVertPath);
    if (pShaderManager->IsLoadReady(entry.sFragPath) == false)
        pShaderManager->RequestLoad(entry.sFragPath);
    if (pShaderManager->IsLoadReady(entry.sVertPath) == false || pShaderManager->IsLoadReady(entry.sFragPath) == false)
        return VK_NULL_HANDLE;

    VkShaderModule modVert = pShaderManager->GetShaderIfReady(device, entry.sVertPath);
    VkShaderModule modFrag = pShaderManager->GetShaderIfReady(device, entry.sFragPath);
    if ((modVert == VK_NULL_HANDLE) || (modFrag == VK_NULL_HANDLE)) {
        if (modVert != VK_NULL_HANDLE)
            pShaderManager->Release(entry.sVertPath);
        if (modFrag != VK_NULL_HANDLE)
            pShaderManager->Release(entry.sFragPath);
        return VK_NULL_HANDLE;
    }
    pShaderManager->Release(entry.sVertPath);
    pShaderManager->Release(entry.sFragPath);

    bool bExtentMatch   = (entry.extent.width == extent.width && entry.extent.height == extent.height);
    bool bRenderPassMatch = (entry.renderPass == renderPass);
    bool bParamsMatch   = (entry.lastParams == pipelineParams);
    if ((entry.pipeline.IsValid() == false) || (bExtentMatch == false) || (bRenderPassMatch == false) || (bParamsMatch == false)) {
        if (entry.pipeline.IsValid() == true)
            entry.pipeline.Destroy();
        entry.pipeline.Create(device, extent, renderPass, pShaderManager, entry.sVertPath, entry.sFragPath, pipelineParams);
        entry.extent     = extent;
        entry.renderPass = renderPass;
        entry.lastParams = pipelineParams;
    }
    return entry.pipeline.Get();
}

VkPipelineLayout PipelineManager::GetPipelineLayoutIfReady(const std::string& sKey) const {
    auto it = this->m_entries.find(sKey);
    if (it == this->m_entries.end())
        return VK_NULL_HANDLE;
    if (it->second.pipeline.IsValid() == false)
        return VK_NULL_HANDLE;
    return it->second.pipeline.GetLayout();
}

void PipelineManager::DestroyPipelines() {
    for (auto& kv : this->m_entries) {
        if (kv.second.pipeline.IsValid() == true)
            kv.second.pipeline.Destroy();
        kv.second.extent     = { 0u, 0u };
        kv.second.renderPass = VK_NULL_HANDLE;
        kv.second.lastParams = {};
    }
}
