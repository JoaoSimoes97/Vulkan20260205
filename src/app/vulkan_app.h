#pragma once

#include "managers/pipeline_manager.h"
#include "thread/job_queue.h"
#include "vulkan_config.h"
#include "vulkan_device.h"
#include "vulkan_framebuffers.h"
#include "vulkan_instance.h"
#include "vulkan_render_pass.h"
#include "vulkan_shader_manager.h"
#include "vulkan_swapchain.h"
#include "window.h"
#include <memory>

/*
 * Main application: owns job queue (for loaders), window, Vulkan instance, device, swapchain,
 * render pass, shader manager, pipeline, framebuffers.
 * Rebuild cases: docs/vulkan/swapchain-rebuild-cases.md.
 */
class VulkanApp {
public:
    VulkanApp();
    ~VulkanApp();

    void Run();

    /* Apply new config at runtime (e.g. from CFG file or UI). Resizes window if size changed; sets bSwapchainDirty so next frame recreates. */
    void ApplyConfig(const VulkanConfig& newConfig);

private:
    void InitWindow();
    void InitVulkan();
    void MainLoop();
    void Cleanup();
    void DrawFrame();
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
};
