/*
 * VulkanApp — main application and frame loop.
 *
 * Owns: window, Vulkan instance/device, swapchain, render pass, pipeline manager,
 * framebuffers, command buffers, sync. Init order and swapchain rebuild flow are
 * documented in docs/architecture.md.
 */
#include "vulkan_app.h"
#include "config_loader.h"
#include "vulkan_utils.h"
#include <SDL3/SDL_stdinc.h>
#include <chrono>
#include <stdexcept>
#include <vector>

// -----------------------------------------------------------------------------
// Constants (paths are resolved relative to executable; see VulkanUtils::GetResourcePath)
// -----------------------------------------------------------------------------
static constexpr int MAIN_LOOP_MAX_SECONDS = 5;  // 0 = no limit (run until quit)
static const char* CONFIG_PATH_USER       = "config/config.json";
static const char* CONFIG_PATH_DEFAULT    = "config/default.json";
static const char* SHADER_VERT_PATH       = "shaders/vert.spv";
static const char* SHADER_FRAG_PATH       = "shaders/frag.spv";
static const char* PIPELINE_KEY_MAIN      = "main";

VulkanApp::VulkanApp() {
    VulkanUtils::LogTrace("VulkanApp constructor");
    std::string userPath   = VulkanUtils::GetResourcePath(CONFIG_PATH_USER);
    std::string defaultPath = VulkanUtils::GetResourcePath(CONFIG_PATH_DEFAULT);
    m_config = LoadConfigFromFileOrCreate(userPath, defaultPath);
    m_jobQueue.Start();
    m_shaderManager.Create(&m_jobQueue);
    InitWindow();
    InitVulkan();
}

VulkanApp::~VulkanApp() {
    VulkanUtils::LogTrace("VulkanApp destructor");
    Cleanup();
}

void VulkanApp::InitWindow() {
    VulkanUtils::LogTrace("InitWindow");
    const char* title = m_config.sWindowTitle.empty() ? "Vulkan App" : m_config.sWindowTitle.c_str();
    m_pWindow = std::make_unique<Window>(m_config.lWidth, m_config.lHeight, title);
}

void VulkanApp::InitVulkan() {
    VulkanUtils::LogTrace("InitVulkan");

    uint32_t extCount = 0;
    const char* const* extNames = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!extNames || extCount == 0) {
        VulkanUtils::LogErr("SDL_Vulkan_GetInstanceExtensions failed or returned no extensions");
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
    }
    std::vector<const char*> extensions(extNames, extNames + extCount);
    if (VulkanUtils::ENABLE_VALIDATION_LAYERS)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    m_instance.Create(extensions.data(), static_cast<uint32_t>(extensions.size()));
    m_pWindow->CreateSurface(m_instance.Get());
    m_device.Create(m_instance.Get(), m_pWindow->GetSurface());

    /* Use window drawable size for swapchain so extent always matches what we display (no aspect mismatch). */
    m_pWindow->GetDrawableSize(&m_config.lWidth, &m_config.lHeight);
    if (m_config.lWidth == 0 || m_config.lHeight == 0) {
        VulkanUtils::LogErr("Window drawable size is 0x0; cannot create swapchain");
        throw std::runtime_error("Window drawable size is zero");
    }
    VulkanUtils::LogInfo("Init: drawable size {}x{}, creating swapchain", m_config.lWidth, m_config.lHeight);
    m_swapchain.Create(m_device.GetDevice(), m_device.GetPhysicalDevice(), m_pWindow->GetSurface(),
                      m_device.GetQueueFamilyIndices(), m_config);
    VkExtent2D initExtent = m_swapchain.GetExtent();
    VulkanUtils::LogInfo("Swapchain extent {}x{}", initExtent.width, initExtent.height);
    m_renderPass.Create(m_device.GetDevice(), m_swapchain.GetImageFormat());

    std::string vertPath = VulkanUtils::GetResourcePath(SHADER_VERT_PATH);
    std::string fragPath = VulkanUtils::GetResourcePath(SHADER_FRAG_PATH);
    m_pipelineManager.RequestPipeline(PIPELINE_KEY_MAIN, &m_shaderManager, vertPath, fragPath);

    m_framebuffers.Create(m_device.GetDevice(), m_renderPass.Get(),
                          m_swapchain.GetImageViews(), m_swapchain.GetExtent());
    m_commandBuffers.Create(m_device.GetDevice(),
                            m_device.GetQueueFamilyIndices().graphicsFamily,
                            m_swapchain.GetImageCount());

    uint32_t maxFramesInFlight = (m_config.lMaxFramesInFlight >= 1u) ? m_config.lMaxFramesInFlight : 1u;
    m_sync.Create(m_device.GetDevice(), maxFramesInFlight, m_swapchain.GetImageCount());
}

void VulkanApp::RecreateSwapchainAndDependents() {
    VulkanUtils::LogTrace("RecreateSwapchainAndDependents");
    VkResult r = vkDeviceWaitIdle(m_device.GetDevice());
    if (r != VK_SUCCESS)
        VulkanUtils::LogErr("vkDeviceWaitIdle before recreate failed: {}", static_cast<int>(r));

    m_framebuffers.Destroy();
    m_pipelineManager.DestroyPipelines();
    m_swapchain.RecreateSwapchain(m_config);
    m_renderPass.Destroy();
    m_renderPass.Create(m_device.GetDevice(), m_swapchain.GetImageFormat());
    m_framebuffers.Create(m_device.GetDevice(), m_renderPass.Get(),
                          m_swapchain.GetImageViews(), m_swapchain.GetExtent());
    m_commandBuffers.Destroy();
    m_commandBuffers.Create(m_device.GetDevice(),
                            m_device.GetQueueFamilyIndices().graphicsFamily,
                            m_swapchain.GetImageCount());
    uint32_t maxFramesInFlight = (m_config.lMaxFramesInFlight >= 1u) ? m_config.lMaxFramesInFlight : 1u;
    m_sync.Destroy();
    m_sync.Create(m_device.GetDevice(), maxFramesInFlight, m_swapchain.GetImageCount());
}

void VulkanApp::MainLoop() {
    VulkanUtils::LogTrace("MainLoop");
    bool quit = false;
    const auto loopStart = std::chrono::steady_clock::now();

    while (!quit) {
        if (MAIN_LOOP_MAX_SECONDS > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - loopStart).count();
            if (elapsed >= MAIN_LOOP_MAX_SECONDS) {
                VulkanUtils::LogInfo("Max run time ({} s) reached, exiting", MAIN_LOOP_MAX_SECONDS);
                break;
            }
        }

        m_jobQueue.ProcessCompletedJobs([](LoadJobType, const std::string&, std::vector<uint8_t>) {});

        quit = m_pWindow->PollEvents();
        if (quit) {
            VulkanUtils::LogTrace("Quitting main loop");
            break;
        }

        if (m_pWindow->GetWindowMinimized()) {
            VulkanUtils::LogTrace("Window minimized, skipping draw");
            continue;
        }

        // Resize: only recreate when extent actually changed (avoids infinite recreate loop).
        if (m_pWindow->GetFramebufferResized()) {
            m_pWindow->SetFramebufferResized(false);
            uint32_t newW = 0, newH = 0;
            m_pWindow->GetDrawableSize(&newW, &newH);
            if (newW == 0 || newH == 0)
                continue;
            VkExtent2D current = m_swapchain.GetExtent();
            if (newW != current.width || newH != current.height) {
                VulkanUtils::LogInfo("Resize: drawable {}x{} -> {}x{}, recreating swapchain",
                    current.width, current.height, newW, newH);
                m_config.lWidth  = newW;
                m_config.lHeight = newH;
                RecreateSwapchainAndDependents();
                VkExtent2D newExtent = m_swapchain.GetExtent();
                VulkanUtils::LogInfo("Swapchain extent {}x{}", newExtent.width, newExtent.height);
            }
        } else if (m_config.bSwapchainDirty) {
            VulkanUtils::LogInfo("Swapchain dirty, recreating (config path)");
            m_config.bSwapchainDirty = false;
            RecreateSwapchainAndDependents();
            VkExtent2D ext = m_swapchain.GetExtent();
            VulkanUtils::LogInfo("Swapchain extent {}x{}", ext.width, ext.height);
        }

        // Pipeline params: frontFace CLOCKWISE matches vertex shader triangle winding so it isn't culled.
        GraphicsPipelineParams pipeParams = {
            .topology                = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable  = VK_FALSE,
            .polygonMode             = VK_POLYGON_MODE_FILL,
            .cullMode                = VK_CULL_MODE_BACK_BIT,
            .frontFace               = VK_FRONT_FACE_CLOCKWISE,
            .lineWidth               = 1.0f,
            .rasterizationSamples    = VK_SAMPLE_COUNT_1_BIT,
        };
        VkPipeline pipeline = m_pipelineManager.GetPipelineIfReady(
            PIPELINE_KEY_MAIN, m_device.GetDevice(), m_renderPass.Get(), &m_shaderManager, pipeParams);
        VkPipelineLayout pipelineLayout = m_pipelineManager.GetPipelineLayoutIfReady(PIPELINE_KEY_MAIN);
        if (pipeline != VK_NULL_HANDLE && pipelineLayout != VK_NULL_HANDLE) {
            /* Orthographic projection with aspect correction so the triangle isn't stretched on non-square windows.
             * Column-major (GLSL): scale(sx, 1, 1) with sx = height/width. */
            VkExtent2D ext = m_swapchain.GetExtent();
            float aspect = (ext.width > 0) ? static_cast<float>(ext.height) / static_cast<float>(ext.width) : 1.f;
            alignas(16) float projMat4[16] = {
                aspect, 0.f, 0.f, 0.f,
                0.f,    1.f, 0.f, 0.f,
                0.f,    0.f, 1.f, 0.f,
                0.f,    0.f, 0.f, 1.f
            };
            constexpr uint32_t kPushConstantSize = 64u;
            std::vector<DrawCall> drawCalls = {
                { .pipeline         = pipeline,
                  .pipelineLayout   = pipelineLayout,
                  .pPushConstants   = projMat4,
                  .pushConstantSize = kPushConstantSize,
                  .vertexCount      = 3,
                  .instanceCount   = 1,
                  .firstVertex     = 0,
                  .firstInstance   = 0 }
            };
            DrawFrame(drawCalls);
        }
    }
}

void VulkanApp::Run() {
    MainLoop();
}

void VulkanApp::ApplyConfig(const VulkanConfig& newConfig) {
    m_config = newConfig;
    if (m_pWindow) {
        uint32_t w = 0, h = 0;
        m_pWindow->GetDrawableSize(&w, &h);
        if (m_config.lWidth != w || m_config.lHeight != h)
            m_pWindow->SetSize(m_config.lWidth, m_config.lHeight);
        m_pWindow->SetFullscreen(m_config.bFullscreen);
        if (!m_config.sWindowTitle.empty())
            m_pWindow->SetTitle(m_config.sWindowTitle.c_str());
    }
    m_config.bSwapchainDirty = true;
}

void VulkanApp::Cleanup() {
    if (m_device.IsValid()) {
        VkResult r = vkDeviceWaitIdle(m_device.GetDevice());
        if (r != VK_SUCCESS)
            VulkanUtils::LogErr("vkDeviceWaitIdle before cleanup failed: {}", static_cast<int>(r));
    }
    m_sync.Destroy();
    m_commandBuffers.Destroy();
    m_framebuffers.Destroy();
    m_pipelineManager.DestroyPipelines();
    m_renderPass.Destroy();
    m_swapchain.Destroy();
    m_device.Destroy();
    if (m_pWindow && m_instance.IsValid())
        m_pWindow->DestroySurface(m_instance.Get());
    m_instance.Destroy();
    m_pWindow.reset();
    m_shaderManager.Destroy();
    m_jobQueue.Stop();
}

void VulkanApp::DrawFrame(const std::vector<DrawCall>& drawCalls) {
    VkDevice device = m_device.GetDevice();
    uint32_t frameIndex = m_sync.GetCurrentFrameIndex();
    VkFence inFlightFence = m_sync.GetInFlightFence(frameIndex);
    VkSemaphore imageAvailable = m_sync.GetImageAvailableSemaphore(frameIndex);

    constexpr uint64_t timeout = UINT64_MAX;
    VkResult r = vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, timeout);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkWaitForFences failed: {}", static_cast<int>(r));
        return;
    }
    r = vkResetFences(device, 1, &inFlightFence);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkResetFences failed: {}", static_cast<int>(r));
        return;
    }

    uint32_t imageIndex = 0;
    r = vkAcquireNextImageKHR(device, m_swapchain.GetSwapchain(), timeout,
                              imageAvailable, VK_NULL_HANDLE, &imageIndex);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchainAndDependents();
        return;
    }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
        VulkanUtils::LogErr("vkAcquireNextImageKHR failed: {}", static_cast<int>(r));
        return;
    }
    if (imageIndex >= m_framebuffers.GetCount() || imageIndex >= m_commandBuffers.GetCount()) {
        VulkanUtils::LogErr("Acquired imageIndex {} out of range", imageIndex);
        RecreateSwapchainAndDependents();
        return;
    }

    VkSemaphore renderFinished = m_sync.GetRenderFinishedSemaphore(imageIndex);
    if (renderFinished == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("No render-finished semaphore for imageIndex {}", imageIndex);
        m_sync.AdvanceFrame();
        return;
    }

    VkClearValue clearColor = {};
    clearColor.color.float32[0] = 0.1f;
    clearColor.color.float32[1] = 0.1f;
    clearColor.color.float32[2] = 0.4f;
    clearColor.color.float32[3] = 1.0f;

    m_commandBuffers.Record(imageIndex, m_renderPass.Get(),
                            m_framebuffers.Get()[imageIndex],
                            m_swapchain.GetExtent(), drawCalls, clearColor);

    VkCommandBuffer cmd = m_commandBuffers.Get(imageIndex);
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = nullptr,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &imageAvailable,
        .pWaitDstStageMask    = &waitStage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &renderFinished,
    };
    r = vkQueueSubmit(m_device.GetGraphicsQueue(), 1, &submitInfo, inFlightFence);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkQueueSubmit failed: {}", static_cast<int>(r));
        RecreateSwapchainAndDependents();
        return;
    }

    VkSwapchainKHR swapchain = m_swapchain.GetSwapchain();
    VkPresentInfoKHR presentInfo = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext              = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &renderFinished,
        .swapchainCount     = 1,
        .pSwapchains        = &swapchain,
        .pImageIndices      = &imageIndex,
        .pResults           = nullptr,
    };
    r = vkQueuePresentKHR(m_device.GetPresentQueue(), &presentInfo);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
        RecreateSwapchainAndDependents();
    else if (r != VK_SUCCESS)
        VulkanUtils::LogErr("vkQueuePresentKHR failed: {}", static_cast<int>(r));

    m_sync.AdvanceFrame();
}
