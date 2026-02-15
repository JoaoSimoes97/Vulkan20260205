#pragma once

#include "camera/camera.h"
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
#include <memory>
#include <vector>

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
    void DrawFrame(const std::vector<DrawCall>& vecDrawCalls_ic);
    void RecreateSwapchainAndDependents();
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

    Camera m_camera;
    float m_avgFrameTimeSec = 1.f / 60.f;
    std::chrono::steady_clock::time_point m_lastFpsTitleUpdate;
};
