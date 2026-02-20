#pragma once

#include <cstddef>
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
#include <tuple>
#include <memory>
#include <string>
#include <vector>

class VulkanShaderManager;

/**
 * Per-object data stored in SSBO for GPU access.
 * Each object gets a 256-byte slot (index * 256 = offset for dynamic binding).
 */
struct ObjectData {
    glm::mat4 model;              // 64 bytes - model matrix for lighting (offset 0)
    glm::vec4 emissive;           // 16 bytes - RGB + strength (offset 64)
    glm::vec4 matProps;           // 16 bytes - x=metallic, y=roughness, z=normalScale, w=occlusionStrength (offset 80)
    glm::vec4 baseColor;          // 16 bytes - RGBA color (offset 96)
    glm::vec4 reserved0;          // 16 bytes - reserved for future (lighting) (offset 112)
    glm::vec4 reserved1;          // 16 bytes - reserved for future (animation) (offset 128)
    glm::vec4 reserved2;          // 16 bytes - reserved for future (physics) (offset 144)
    glm::vec4 reserved3;          // 16 bytes - reserved for future (particles) (offset 160)
    glm::vec4 reserved4;          // 16 bytes - reserved for future (phase 3B) (offset 176)
    glm::vec4 reserved5;          // 16 bytes - reserved for future (UI/effects) (offset 192)
    glm::vec4 reserved6;          // 16 bytes - reserved for future (audio/events) (offset 208)
    glm::vec4 reserved7;          // 16 bytes - reserved for future (custom) (offset 224)
    glm::vec4 reserved8;          // 16 bytes - reserved for future expansion (offset 240)
    // Total: 256 bytes (64 + 12*vec4 = 64 + 192 = 256)
};

// ObjectData layout validations (MUST match GLSL ObjectData struct)
constexpr size_t kObjDataOffset_Model     = 0;
constexpr size_t kObjDataOffset_Emissive  = 64;
constexpr size_t kObjDataOffset_MatProps  = 80;
constexpr size_t kObjDataOffset_BaseColor = 96;
static_assert(sizeof(ObjectData) == 256, "ObjectData must be 256 bytes");
static_assert(offsetof(ObjectData, model) == kObjDataOffset_Model, "model must be at offset 0");
static_assert(offsetof(ObjectData, emissive) == kObjDataOffset_Emissive, "emissive must be at offset 64");
static_assert(offsetof(ObjectData, matProps) == kObjDataOffset_MatProps, "matProps must be at offset 80");
static_assert(offsetof(ObjectData, baseColor) == kObjDataOffset_BaseColor, "baseColor must be at offset 96");

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
    
    /** Get or create a descriptor set for a single texture. Returns VK_NULL_HANDLE on failure. Caches result. */
    VkDescriptorSet GetOrCreateDescriptorSetForTexture(std::shared_ptr<TextureHandle> pTexture);
    
    /** Get or create a descriptor set for the given textures. Returns VK_NULL_HANDLE on failure. Caches result. */
    VkDescriptorSet GetOrCreateDescriptorSetForTextures(std::shared_ptr<TextureHandle> pBaseColorTexture,
                                                        std::shared_ptr<TextureHandle> pMetallicRoughnessTexture,
                                                        std::shared_ptr<TextureHandle> pEmissiveTexture,
                                                        std::shared_ptr<TextureHandle> pNormalTexture,
                                                        std::shared_ptr<TextureHandle> pOcclusionTexture);
    
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
    /** Per-texture-quintuple descriptor set cache: (baseColor, metallicRoughness, emissive, normal, occlusion) -> descriptor set. */
    std::map<std::tuple<TextureHandle*, TextureHandle*, TextureHandle*, TextureHandle*, TextureHandle*>, VkDescriptorSet> m_textureQuintupleDescriptorSets;
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
