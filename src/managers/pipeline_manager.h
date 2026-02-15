#pragma once

#include "vulkan/vulkan_pipeline.h"
#include "vulkan/vulkan_shader_manager.h"
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

/*
 * Handle that owns a VulkanPipeline. Materials hold shared_ptr<PipelineHandle> so pipelines
 * stay alive while any object uses that material. Destroy() is called at a safe time
 * (after fence wait), not in the destructor.
 */
struct PipelineHandle {
    void Create(VkDevice device, VkRenderPass renderPass,
                VulkanShaderManager* pShaderManager,
                const std::string& sVertPath, const std::string& sFragPath,
                const GraphicsPipelineParams& pipelineParams,
                const PipelineLayoutDescriptor& layoutDescriptor,
                bool renderPassHasDepth);
    void Destroy();
    VkPipeline Get() const { return m_pipeline.Get(); }
    VkPipelineLayout GetLayout() const { return m_pipeline.GetLayout(); }
    bool IsValid() const { return m_pipeline.IsValid(); }
private:
    VulkanPipeline m_pipeline;
};

/*
 * Pipeline manager: request pipelines by key; returns shared_ptr<PipelineHandle>. TrimUnused()
 * moves unused handles to a pending list; call ProcessPendingDestroys() after vkWaitForFences
 * to destroy them safely. DestroyPipelines() on swapchain recreate.
 */
class PipelineManager {
public:
    PipelineManager() = default;

    void RequestPipeline(const std::string& sKey,
                         VulkanShaderManager* pShaderManager,
                         const std::string& sVertPath,
                         const std::string& sFragPath);

    /** Non-blocking: return shared_ptr<PipelineHandle> when shaders ready and pipeline built; else nullptr. */
    std::shared_ptr<PipelineHandle> GetPipelineHandleIfReady(const std::string& sKey,
                                                             VkDevice device,
                                                             VkRenderPass renderPass,
                                                             VulkanShaderManager* pShaderManager,
                                                             const GraphicsPipelineParams& pipelineParams,
                                                             const PipelineLayoutDescriptor& layoutDescriptor,
                                                             bool renderPassHasDepth);

    /** Remove cache entries where use_count() == 1; moved to pending destroy. Call once per frame. */
    void TrimUnused();

    /** Destroy pipelines that were trimmed. Call at start of frame after vkWaitForFences. */
    void ProcessPendingDestroys();

    void DestroyPipelines();

private:
    struct PipelineEntry {
        std::string                    sVertPath;
        std::string                    sFragPath;
        std::shared_ptr<PipelineHandle> handle;
        VkRenderPass                   renderPass = VK_NULL_HANDLE;
        GraphicsPipelineParams         lastParams = {};
        PipelineLayoutDescriptor       lastLayout = {};
        bool                           lastRenderPassHasDepth = false;
    };
    std::map<std::string, PipelineEntry> m_entries;
    std::vector<std::shared_ptr<PipelineHandle>> m_pendingDestroy;
};
