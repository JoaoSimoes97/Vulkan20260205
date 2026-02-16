#pragma once

#include "camera/camera.h"
#include "managers/descriptor_pool_manager.h"
#include "managers/descriptor_set_layout_manager.h"
#include "managers/material_manager.h"
#include "managers/mesh_manager.h"
#include "managers/texture_manager.h"
#include "managers/pipeline_manager.h"
#include "managers/scene_manager.h"
#include "render/render_list_builder.h"
#include "thread/job_queue.h"
#include "vulkan_config.h"
#include "vulkan_command_buffers.h"
#include "vulkan_depth_image.h"
#include "vulkan_device.h"
#include "vulkan_framebuffers.h"
#include "vulkan_instance.h"
#include "vulkan_render_pass.h"
#include "vulkan_shader_manager.h"
#include "vulkan_sync.h"
#include "vulkan_swapchain.h"
#include "window.h"
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

/**
 * Main application: owns window, Vulkan instance/device/swapchain, render pass,
 * pipeline/material/mesh managers, scene, camera, and frame loop.
 * See docs/architecture.md for init order and swapchain rebuild flow.
 */
class VulkanApp {
public:
    VulkanApp();
    ~VulkanApp();

    void Run();
    void ApplyConfig(const VulkanConfig& stNewConfig_ic);

private:
    void InitWindow();
    void InitVulkan();
    void MainLoop();
    void Cleanup();
    /** Returns false on fatal error (e.g. device lost); caller should exit the loop. */
    bool DrawFrame(const std::vector<DrawCall>& vecDrawCalls_ic);
    void RecreateSwapchainAndDependents();
    /** Write default texture into the main descriptor set when ready; then add main/wire to m_pipelineDescriptorSets. Idempotent. */
    void EnsureMainDescriptorSetWritten();
    void OnCompletedLoadJob(LoadJobType eType_ic, const std::string& sPath_ic, std::vector<uint8_t> vecData_in);

    JobQueue::CompletedJobHandler m_completedJobHandler;
    VulkanConfig m_config;
    JobQueue m_jobQueue;
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
    VkDescriptorSet           m_descriptorSetMain = VK_NULL_HANDLE;  /* single set for "main" pipeline (default texture) */
    /** Keep default texture alive so TrimUnused() does not destroy it (descriptor set references its view/sampler). */
    std::shared_ptr<TextureHandle> m_pDefaultTexture;

    Camera m_camera;
    float m_avgFrameTimeSec = 1.f / 60.f;
    std::chrono::steady_clock::time_point m_lastFpsTitleUpdate;
};
