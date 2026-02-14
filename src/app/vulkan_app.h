#pragma once

#include "managers/pipeline_manager.h"
#include "thread/job_queue.h"
#include "vulkan_config.h"
#include "vulkan_command_buffers.h"
#include "vulkan_device.h"
#include "vulkan_framebuffers.h"
#include "vulkan_instance.h"
#include "scene/object.h"
#include "vulkan_render_pass.h"
#include "vulkan_shader_manager.h"
#include "vulkan_sync.h"
#include "vulkan_swapchain.h"
#include "window.h"
#include <memory>
#include <vector>

/**
 * Main application: owns window, Vulkan instance/device/swapchain, render pass, pipeline manager,
 * framebuffers, command buffers, sync, and job queue (async shader loads).
 * See docs/architecture.md for init order and swapchain rebuild flow.
 */
class VulkanApp {
public:
    VulkanApp();
    ~VulkanApp();

    void Run();

    /** Apply config at runtime; resizes window if needed and sets bSwapchainDirty for next-frame recreate. */
    void ApplyConfig(const VulkanConfig& newConfig);

private:
    void InitWindow();
    void InitVulkan();
    void MainLoop();
    void Cleanup();
    void DrawFrame(const std::vector<DrawCall>& drawCalls);
    void RecreateSwapchainAndDependents();

    VulkanConfig m_config;
    JobQueue m_jobQueue;
    VulkanShaderManager m_shaderManager;
    std::unique_ptr<Window> m_pWindow;
    VulkanInstance m_instance;
    VulkanDevice m_device;
    VulkanSwapchain m_swapchain;
    VulkanRenderPass m_renderPass;
    PipelineManager m_pipelineManager;
    VulkanFramebuffers m_framebuffers;
    VulkanCommandBuffers m_commandBuffers;
    VulkanSync m_sync;
    std::vector<Object> m_objects;
};
