/*
 * PipelineManager — request pipelines by key (vert+frag paths). GetPipelineIfReady is non-blocking;
 * returns VK_NULL_HANDLE until shaders are loaded, then builds or reuses pipeline for renderPass/params/layout.
 */
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
                                               VkRenderPass renderPass,
                                               VulkanShaderManager* pShaderManager,
                                               const GraphicsPipelineParams& pipelineParams,
                                               const PipelineLayoutDescriptor& layoutDescriptor) {
    if (device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || pShaderManager == nullptr)
        return VK_NULL_HANDLE;
    auto it = this->m_entries.find(sKey);
    if (it == this->m_entries.end())
        return VK_NULL_HANDLE;

    PipelineEntry& entry = it->second;
    if (!pShaderManager->IsLoadReady(entry.sVertPath))
        pShaderManager->RequestLoad(entry.sVertPath);
    if (!pShaderManager->IsLoadReady(entry.sFragPath))
        pShaderManager->RequestLoad(entry.sFragPath);
    if (!pShaderManager->IsLoadReady(entry.sVertPath) || !pShaderManager->IsLoadReady(entry.sFragPath))
        return VK_NULL_HANDLE;

    VkShaderModule modVert = pShaderManager->GetShaderIfReady(device, entry.sVertPath);
    VkShaderModule modFrag = pShaderManager->GetShaderIfReady(device, entry.sFragPath);
    if (modVert == VK_NULL_HANDLE || modFrag == VK_NULL_HANDLE) {
        if (modVert != VK_NULL_HANDLE)
            pShaderManager->Release(entry.sVertPath);
        if (modFrag != VK_NULL_HANDLE)
            pShaderManager->Release(entry.sFragPath);
        return VK_NULL_HANDLE;
    }
    pShaderManager->Release(entry.sVertPath);
    pShaderManager->Release(entry.sFragPath);

    bool renderPassMatch = (entry.renderPass == renderPass);
    bool paramsMatch    = (entry.lastParams == pipelineParams);
    bool layoutMatch    = (entry.lastLayout == layoutDescriptor);
    if (!entry.pipeline.IsValid() || !renderPassMatch || !paramsMatch || !layoutMatch) {
        if (entry.pipeline.IsValid())
            entry.pipeline.Destroy();
        entry.pipeline.Create(device, renderPass, pShaderManager, entry.sVertPath, entry.sFragPath, pipelineParams, layoutDescriptor);
        entry.renderPass = renderPass;
        entry.lastParams = pipelineParams;
        entry.lastLayout = layoutDescriptor;
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
        if (kv.second.pipeline.IsValid())
            kv.second.pipeline.Destroy();
        kv.second.renderPass = VK_NULL_HANDLE;
        kv.second.lastParams = {};
        kv.second.lastLayout = {};
    }
}
