#pragma once

#include "camera.h"
#include "managers/descriptor_pool_manager.h"
#include "managers/descriptor_set_layout_manager.h"
#include "managers/material_manager.h"
#include "managers/mesh_manager.h"
#include "managers/pipeline_manager.h"
#include "managers/scene_manager.h"
#include "managers/texture_manager.h"
#include "render/render_list_builder.h"
#include "thread/job_queue.h"
#include "thread/resource_manager_thread.h"
#include "vulkan_config.h"
#include "vulkan_command_buffers.h"
#include "vulkan_depth_image.h"
#include "vulkan_device.h"
#include "vulkan_framebuffers.h"
#include "vulkan_instance.h"
#include "vulkan_render_pass.h"
#include "vulkan_swapchain.h"
#include "vulkan_sync.h"
#include "window/window.h"
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

class VulkanShaderManager;

class VulkanApp {
public:
    explicit VulkanApp(const VulkanConfig& config_in);
    ~VulkanApp();

    void Run();

private:
    void InitWindow();
    void InitVulkan();
    void MainLoop();
    void Cleanup();

    /**
     * DrawFrame: record and present. Returns false on fatal error (e.g. device lost); caller should exit the loop.
     */
    bool DrawFrame(const std::vector<DrawCall>& vecDrawCalls_ic);
    void RecreateSwapchainAndDependents();
    /** Write default texture into the main descriptor set when ready; then add main/wire to m_pipelineDescriptorSets. Idempotent. */
    void EnsureMainDescriptorSetWritten();
    
    /** Get or create a descriptor set for the given texture. Returns VK_NULL_HANDLE on failure. Caches result. */
    VkDescriptorSet GetOrCreateDescriptorSetForTexture(std::shared_ptr<TextureHandle> pTexture);
    
    /** Clean up descriptor sets for textures that are no longer referenced by any objects. Call after scene changes. */
    void CleanupUnusedTextureDescriptorSets();
    void OnCompletedLoadJob(LoadJobType eType_ic, const std::string& sPath_ic, std::vector<uint8_t> vecData_in);
    void ApplyConfig(const VulkanConfig& stNewConfig_ic);

    JobQueue::CompletedJobHandler m_completedJobHandler;
    VulkanConfig m_config;
    JobQueue m_jobQueue;
    ResourceManagerThread m_resourceManagerThread;
    VulkanShaderManager m_shaderManager;
    std::unique_ptr<Window> m_pWindow;
    VulkanInstance m_instance;
    VulkanDevice m_device;
    VulkanSwapchain m_swapchain;
    VulkanRenderPass m_renderPass;
    VulkanDepthImage m_depthImage;
    PipelineManager m_pipelineManager;
    MaterialManager m_materialManager;
    MeshManager m_meshManager;
    TextureManager m_textureManager;
    SceneManager m_sceneManager;
    RenderListBuilder m_renderListBuilder;
    std::vector<DrawCall> m_drawCalls;
    VulkanFramebuffers m_framebuffers;
    VulkanCommandBuffers m_commandBuffers;
    VulkanSync m_sync;

    /* Descriptor layouts and pool (data-driven by layout keys). Sets allocated from pool; map passed to render list. */
    DescriptorSetLayoutManager m_descriptorSetLayoutManager;
    DescriptorPoolManager     m_descriptorPoolManager;
    std::map<std::string, std::vector<VkDescriptorSet>> m_pipelineDescriptorSets;
    VkDescriptorSet           m_descriptorSetMain = VK_NULL_HANDLE;  /* single set for textured pipelines (default texture) */
    /** Keep default texture alive so TrimUnused() does not destroy it (textured descriptor sets use its view/sampler). */
    std::shared_ptr<TextureHandle> m_pDefaultTexture;
    /** Keep material references alive so TrimUnused() does not destroy them. */
    std::vector<std::shared_ptr<MaterialHandle>> m_cachedMaterials;
    /** Per-texture descriptor set cache: texture -> descriptor set. */
    std::map<TextureHandle*, VkDescriptorSet> m_textureDescriptorSets;
    /** Reverse map: descriptor set -> texture (for reference counting and cleanup). */
    std::map<VkDescriptorSet, std::shared_ptr<TextureHandle>> m_descriptorSetTextures;

    Camera m_camera;
    float m_avgFrameTimeSec = 1.f / 60.f;
    std::chrono::steady_clock::time_point m_lastFpsTitleUpdate;
};
