/*
 * PipelineManager — request pipelines by key; returns shared_ptr<PipelineHandle>. TrimUnused
 * moves unused to pending; ProcessPendingDestroys() at safe time (after fence wait).
 */
#include "pipeline_manager.h"

void PipelineHandle::Create(VkDevice device, VkRenderPass renderPass,
                            VulkanShaderManager* pShaderManager,
                            const std::string& sVertPath, const std::string& sFragPath,
                            const GraphicsPipelineParams& pipelineParams,
                            const PipelineLayoutDescriptor& layoutDescriptor,
                            bool renderPassHasDepth) {
    m_pipeline.Create(device, renderPass, pShaderManager, sVertPath, sFragPath,
                      pipelineParams, layoutDescriptor, renderPassHasDepth);
}

void PipelineHandle::Destroy() {
    m_pipeline.Destroy();
}

void PipelineManager::RequestPipeline(const std::string& sKey,
                                       VulkanShaderManager* pShaderManager,
                                       const std::string& sVertPath,
                                       const std::string& sFragPath) {
    if (pShaderManager == nullptr || !pShaderManager->IsValid())
        return;
    auto it = m_entries.find(sKey);
    if (it != m_entries.end())
        return;
    m_entries[sKey].sVertPath = sVertPath;
    m_entries[sKey].sFragPath = sFragPath;
    pShaderManager->RequestLoad(sVertPath);
    pShaderManager->RequestLoad(sFragPath);
}

std::shared_ptr<PipelineHandle> PipelineManager::GetPipelineHandleIfReady(const std::string& sKey,
                                                                          VkDevice device,
                                                                          VkRenderPass renderPass,
                                                                          VulkanShaderManager* pShaderManager,
                                                                          const GraphicsPipelineParams& pipelineParams,
                                                                          const PipelineLayoutDescriptor& layoutDescriptor,
                                                                          bool renderPassHasDepth) {
    if (device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || pShaderManager == nullptr)
        return nullptr;
    auto it = m_entries.find(sKey);
    if (it == m_entries.end())
        return nullptr;

    PipelineEntry& entry = it->second;
    if (!pShaderManager->IsLoadReady(entry.sVertPath))
        pShaderManager->RequestLoad(entry.sVertPath);
    if (!pShaderManager->IsLoadReady(entry.sFragPath))
        pShaderManager->RequestLoad(entry.sFragPath);
    if (!pShaderManager->IsLoadReady(entry.sVertPath) || !pShaderManager->IsLoadReady(entry.sFragPath))
        return nullptr;

    ShaderModulePtr pVert = pShaderManager->GetShaderIfReady(device, entry.sVertPath);
    ShaderModulePtr pFrag = pShaderManager->GetShaderIfReady(device, entry.sFragPath);
    if (!pVert || !pFrag)
        return nullptr;

    bool renderPassMatch = (entry.renderPass == renderPass);
    bool paramsMatch    = (entry.lastParams == pipelineParams);
    bool layoutMatch    = (entry.lastLayout == layoutDescriptor);
    bool depthMatch     = (entry.lastRenderPassHasDepth == renderPassHasDepth);
    if (!entry.handle || !entry.handle->IsValid() || !renderPassMatch || !paramsMatch || !layoutMatch || !depthMatch) {
        if (entry.handle && entry.handle->IsValid()) {
            m_pendingDestroy.push_back(std::move(entry.handle));
            entry.handle.reset();
        }
        entry.handle = std::make_shared<PipelineHandle>();
        entry.handle->Create(device, renderPass, pShaderManager, entry.sVertPath, entry.sFragPath,
                            pipelineParams, layoutDescriptor, renderPassHasDepth);
        entry.renderPass = renderPass;
        entry.lastParams = pipelineParams;
        entry.lastLayout = layoutDescriptor;
        entry.lastRenderPassHasDepth = renderPassHasDepth;
    }
    return entry.handle;
}

void PipelineManager::TrimUnused() {
    for (auto it = m_entries.begin(); it != m_entries.end(); ) {
        if (it->second.handle && it->second.handle.use_count() == 1u) {
            m_pendingDestroy.push_back(std::move(it->second.handle));
            it = m_entries.erase(it);
        } else {
            ++it;
        }
    }
}

void PipelineManager::ProcessPendingDestroys() {
    for (auto& p : m_pendingDestroy) {
        if (p && p->IsValid())
            p->Destroy();
    }
    m_pendingDestroy.clear();
}

void PipelineManager::DestroyPipelines() {
    for (auto& p : m_pendingDestroy) {
        if (p && p->IsValid())
            p->Destroy();
    }
    m_pendingDestroy.clear();
    for (auto& kv : m_entries) {
        if (kv.second.handle && kv.second.handle->IsValid())
            kv.second.handle->Destroy();
        kv.second.handle.reset();
        kv.second.renderPass = VK_NULL_HANDLE;
        kv.second.lastParams = {};
        kv.second.lastLayout = {};
        kv.second.lastRenderPassHasDepth = false;
    }
}
