/*
 * MaterialManager — registry material id -> shared_ptr<MaterialHandle>. Materials resolve to
 * VkPipeline/VkPipelineLayout via PipelineManager and cache shared_ptr<PipelineHandle>. TrimUnused() drops materials no object uses.
 */
#include "material_manager.h"
#include "pipeline_manager.h"

MaterialHandle::MaterialHandle(std::string key, PipelineLayoutDescriptor layout, GraphicsPipelineParams params)
    : pipelineKey(std::move(key)), layoutDescriptor(std::move(layout)), pipelineParams(params) {}

VkPipeline MaterialHandle::GetPipelineIfReady(VkDevice device,
                                              VkRenderPass renderPass,
                                              PipelineManager* pPipelineManager,
                                              VulkanShaderManager* pShaderManager,
                                              bool renderPassHasDepth) {
    if (pPipelineManager == nullptr || pShaderManager == nullptr)
        return VK_NULL_HANDLE;
    m_pCachedPipeline = pPipelineManager->GetPipelineHandleIfReady(pipelineKey, device, renderPass, pShaderManager,
                                                                   pipelineParams, layoutDescriptor, renderPassHasDepth);
    if (!m_pCachedPipeline || !m_pCachedPipeline->IsValid())
        return VK_NULL_HANDLE;
    return m_pCachedPipeline->Get();
}

VkPipelineLayout MaterialHandle::GetPipelineLayoutIfReady(PipelineManager* pPipelineManager) {
    if (pPipelineManager == nullptr)
        return VK_NULL_HANDLE;
    if (!m_pCachedPipeline || !m_pCachedPipeline->IsValid())
        return VK_NULL_HANDLE;
    return m_pCachedPipeline->GetLayout();
}

std::shared_ptr<MaterialHandle> MaterialManager::RegisterMaterial(const std::string& sMaterialId,
                                                                    const std::string& sPipelineKey,
                                                                    const PipelineLayoutDescriptor& layoutDescriptor,
                                                                    const GraphicsPipelineParams& pipelineParams) {
    auto it = this->m_registry.find(sMaterialId);
    if (it != this->m_registry.end())
        return it->second;
    std::shared_ptr<MaterialHandle> pHandle = std::make_shared<MaterialHandle>(sPipelineKey, layoutDescriptor, pipelineParams);
    this->m_registry[sMaterialId] = pHandle;
    return pHandle;
}

std::shared_ptr<MaterialHandle> MaterialManager::GetMaterial(const std::string& sMaterialId) const {
    auto it = this->m_registry.find(sMaterialId);
    if (it == this->m_registry.end())
        return nullptr;
    return it->second;
}

void MaterialManager::TrimUnused() {
    for (auto it = this->m_registry.begin(); it != this->m_registry.end(); ) {
        if (it->second.use_count() == 1u)
            it = this->m_registry.erase(it);
        else
            ++it;
    }
}
