#pragma once

#include <glm/glm.hpp>
#include "camera.h"
#include "core/light_component.h"
#include "core/light_manager.h"
#include "core/light_debug_renderer.h"
#include "managers/descriptor_pool_manager.h"
#include "managers/descriptor_set_layout_manager.h"
#include "managers/material_manager.h"
#include "managers/mesh_manager.h"
#include "managers/pipeline_manager.h"
#include "managers/resource_cleanup_manager.h"
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

/**
 * Per-object data stored in SSBO for GPU access.
 * Each object gets a 256-byte slot (index * 256 = offset for dynamic binding).
 */
struct ObjectData {
    glm::mat4 model;              // 64 bytes - model matrix for lighting
    glm::vec4 emissive;           // 16 bytes - RGB + strength
    glm::vec4 matProps;           // 16 bytes - metallic, roughness, reserved x2
    glm::vec4 baseColor;          // 16 bytes - RGBA color
    glm::vec4 reserved0;          // 16 bytes - reserved for future (lighting)
    glm::vec4 reserved1;          // 16 bytes - reserved for future (animation)
    glm::vec4 reserved2;          // 16 bytes - reserved for future (physics)
    glm::vec4 reserved3;          // 16 bytes - reserved for future (particles)
    glm::vec4 reserved4;          // 16 bytes - reserved for future (phase 3B)
    glm::vec4 reserved5;          // 16 bytes - reserved for future (UI/effects)
    glm::vec4 reserved6;          // 16 bytes - reserved for future (audio/events)
    glm::vec4 reserved7;          // 16 bytes - reserved for future (custom)
    glm::vec4 reserved8;          // 16 bytes - reserved for future expansion
    // Total: 256 bytes (64 + 12*vec4 = 64 + 192 = 256)
};

static_assert(sizeof(ObjectData) == 256, "ObjectData must be 256 bytes");

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
     * @param viewProjMat16_ic 16-float view-projection matrix for debug rendering (may be null).
     */
    bool DrawFrame(const std::vector<DrawCall>& vecDrawCalls_ic, const float* pViewProjMat16_ic);
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
    ResourceCleanupManager m_resourceCleanupManager;
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
    /** Per-object data SSBO buffer (4096 objects × 256 bytes = 1MB). Written each frame. */
    VkBuffer m_objectDataBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_objectDataMemory = VK_NULL_HANDLE;
    
    /** Light data SSBO buffer (16 byte header + 256 lights × 64 bytes = ~16KB). 
        Note: We have both raw buffer (created early) and LightManager (manages upload).
        The LightManager uses its own buffer internally which is bound to the descriptor set. */
    VkBuffer m_lightBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_lightBufferMemory = VK_NULL_HANDLE;
    
    /** Light manager for uploading scene lights to GPU. */
    LightManager m_lightManager;
    
    /** Light debug renderer for visualizing lights in debug mode. */
    LightDebugRenderer m_lightDebugRenderer;

    Camera m_camera;
    float m_avgFrameTimeSec = 1.f / 60.f;
    std::chrono::steady_clock::time_point m_lastFpsTitleUpdate;
};
