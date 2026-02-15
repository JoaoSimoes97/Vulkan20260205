/*
 * VulkanApp — main application and frame loop.
 *
 * Owns: window, Vulkan instance/device, swapchain, render pass, pipeline manager,
 * framebuffers, command buffers, sync. Init order and swapchain rebuild flow are
 * documented in docs/architecture.md.
 */
#include "vulkan_app.h"
#include "config_loader.h"
#include "camera/camera_controller.h"
#include "scene/object.h"
#include "vulkan/vulkan_utils.h"
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_stdinc.h>
#include <array>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

static const char* CONFIG_PATH_USER     = "config/config.json";
static const char* CONFIG_PATH_DEFAULT  = "config/default.json";
static const char* SHADER_VERT_PATH     = "shaders/vert.spv";
static const char* SHADER_FRAG_PATH     = "shaders/frag.spv";
static const char* SHADER_FRAG_ALT_PATH = "shaders/frag_alt.spv";
static const char* PIPELINE_KEY_MAIN    = "main";
static const char* PIPELINE_KEY_WIRE    = "wire";
static const char* PIPELINE_KEY_ALT     = "alt";
static constexpr float kDefaultPanSpeed = 0.012f;
static constexpr float kOrthoFallbackHalfExtent = 8.f;

VulkanApp::VulkanApp() {
    VulkanUtils::LogTrace("VulkanApp constructor");
    std::string userPath   = VulkanUtils::GetResourcePath(CONFIG_PATH_USER);
    std::string defaultPath = VulkanUtils::GetResourcePath(CONFIG_PATH_DEFAULT);
    m_config = LoadConfigFromFileOrCreate(userPath, defaultPath);
    m_camera.SetPosition(m_config.fInitialCameraX, m_config.fInitialCameraY, m_config.fInitialCameraZ);
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

    const VkFormat depthCandidates[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
    VkFormat depthFormat = VulkanDepthImage::FindSupportedFormat(m_device.GetPhysicalDevice(), depthCandidates, static_cast<uint32_t>(sizeof(depthCandidates) / sizeof(depthCandidates[0])));
    RenderPassDescriptor rpDesc = {
        .colorFormat       = m_swapchain.GetImageFormat(),
        .colorLoadOp       = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .colorStoreOp      = VK_ATTACHMENT_STORE_OP_STORE,
        .colorFinalLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .depthFormat       = depthFormat,
        .depthLoadOp       = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .depthStoreOp      = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .depthFinalLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .sampleCount      = VK_SAMPLE_COUNT_1_BIT,
    };
    m_renderPass.Create(m_device.GetDevice(), rpDesc);
    if (depthFormat != VK_FORMAT_UNDEFINED)
        m_depthImage.Create(m_device.GetDevice(), m_device.GetPhysicalDevice(), depthFormat, initExtent);

    std::string vertPath   = VulkanUtils::GetResourcePath(SHADER_VERT_PATH);
    std::string fragPath   = VulkanUtils::GetResourcePath(SHADER_FRAG_PATH);
    std::string fragAltPath = VulkanUtils::GetResourcePath(SHADER_FRAG_ALT_PATH);
    m_pipelineManager.RequestPipeline(PIPELINE_KEY_MAIN, &m_shaderManager, vertPath, fragPath);
    m_pipelineManager.RequestPipeline(PIPELINE_KEY_WIRE, &m_shaderManager, vertPath, fragPath);
    m_pipelineManager.RequestPipeline(PIPELINE_KEY_ALT, &m_shaderManager, vertPath, fragAltPath);

    constexpr uint32_t kMainPushConstantSize = kObjectPushConstantSize;
    PipelineLayoutDescriptor mainLayoutDesc = {
        .pushConstantRanges = {
            { .stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), .offset = 0u, .size = kMainPushConstantSize }
        }
    };
    GraphicsPipelineParams pipeParamsMain = {
        .topology                = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable  = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = m_config.bCullBackFaces ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE,
        .frontFace               = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth               = 1.0f,
        .rasterizationSamples    = VK_SAMPLE_COUNT_1_BIT,
    };
    GraphicsPipelineParams pipeParamsWire = pipeParamsMain;
    pipeParamsWire.polygonMode = VK_POLYGON_MODE_LINE;
    m_materialManager.RegisterMaterial("main", PIPELINE_KEY_MAIN, mainLayoutDesc, pipeParamsMain);
    m_materialManager.RegisterMaterial("wire", PIPELINE_KEY_WIRE, mainLayoutDesc, pipeParamsWire);
    m_materialManager.RegisterMaterial("alt",  PIPELINE_KEY_ALT,  mainLayoutDesc, pipeParamsMain);
    m_meshManager.SetDevice(m_device.GetDevice());
    m_meshManager.SetPhysicalDevice(m_device.GetPhysicalDevice());
    m_meshManager.SetQueue(m_device.GetGraphicsQueue());
    m_meshManager.SetQueueFamilyIndex(m_device.GetQueueFamilyIndices().graphicsFamily);
    (void)m_meshManager.GetOrCreateProcedural("triangle");
    (void)m_meshManager.GetOrCreateProcedural("circle");
    (void)m_meshManager.GetOrCreateProcedural("rectangle");
    (void)m_meshManager.GetOrCreateProcedural("cube");

    m_sceneManager.SetDependencies(&m_jobQueue, &m_materialManager, &m_meshManager);
    m_meshManager.SetJobQueue(&m_jobQueue);
    m_sceneManager.SetCurrentScene(m_sceneManager.CreateDefaultScene());

    m_framebuffers.Create(m_device.GetDevice(), m_renderPass.Get(),
                          m_swapchain.GetImageViews(),
                          m_depthImage.IsValid() ? m_depthImage.GetView() : VK_NULL_HANDLE,
                          m_swapchain.GetExtent());
    m_commandBuffers.Create(m_device.GetDevice(),
                            m_device.GetQueueFamilyIndices().graphicsFamily,
                            m_swapchain.GetImageCount());

    uint32_t maxFramesInFlight = (m_config.lMaxFramesInFlight >= 1u) ? m_config.lMaxFramesInFlight : 1u;
    m_sync.Create(m_device.GetDevice(), maxFramesInFlight, m_swapchain.GetImageCount());

}

void VulkanApp::RecreateSwapchainAndDependents() {
    VulkanUtils::LogTrace("RecreateSwapchainAndDependents");
    /* Always use current window drawable size so aspect ratio matches after resize or OUT_OF_DATE. */
    if (m_pWindow) {
        uint32_t w = 0, h = 0;
        m_pWindow->GetDrawableSize(&w, &h);
        if (w > 0 && h > 0) {
            m_config.lWidth  = w;
            m_config.lHeight = h;
        }
    }
    VkResult r = vkDeviceWaitIdle(m_device.GetDevice());
    if (r != VK_SUCCESS)
        VulkanUtils::LogErr("vkDeviceWaitIdle before recreate failed: {}", static_cast<int>(r));

    m_framebuffers.Destroy();
    m_depthImage.Destroy();
    m_pipelineManager.DestroyPipelines();
    m_swapchain.RecreateSwapchain(m_config);
    VkExtent2D extent = m_swapchain.GetExtent();
    const VkFormat depthCandidates[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
    VkFormat depthFormat = VulkanDepthImage::FindSupportedFormat(m_device.GetPhysicalDevice(), depthCandidates, static_cast<uint32_t>(sizeof(depthCandidates) / sizeof(depthCandidates[0])));
    RenderPassDescriptor rpDesc = {
        .colorFormat       = m_swapchain.GetImageFormat(),
        .colorLoadOp       = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .colorStoreOp      = VK_ATTACHMENT_STORE_OP_STORE,
        .colorFinalLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .depthFormat       = depthFormat,
        .depthLoadOp       = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .depthStoreOp      = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .depthFinalLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .sampleCount      = VK_SAMPLE_COUNT_1_BIT,
    };
    m_renderPass.Destroy();
    m_renderPass.Create(m_device.GetDevice(), rpDesc);
    if (depthFormat != VK_FORMAT_UNDEFINED)
        m_depthImage.Create(m_device.GetDevice(), m_device.GetPhysicalDevice(), depthFormat, extent);
    m_framebuffers.Create(m_device.GetDevice(), m_renderPass.Get(),
                          m_swapchain.GetImageViews(),
                          m_depthImage.IsValid() ? m_depthImage.GetView() : VK_NULL_HANDLE,
                          extent);
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
    while (!quit) {
        const auto frameStart = std::chrono::steady_clock::now();

        m_jobQueue.ProcessCompletedJobs([this](LoadJobType type, const std::string& path, std::vector<uint8_t> data) {
            m_sceneManager.OnCompletedLoad(type, path, data);
            m_meshManager.OnCompletedMeshFile(path, std::move(data));
        });
        m_shaderManager.TrimUnused();
        m_pipelineManager.TrimUnused();
        m_materialManager.TrimUnused();
        m_meshManager.TrimUnused();

        quit = m_pWindow->PollEvents();
        if (quit)
            break;

        const float panSpeed = (m_config.fPanSpeed > 0.f) ? m_config.fPanSpeed : kDefaultPanSpeed;
        CameraController_Update(m_camera, SDL_GetKeyboardState(nullptr), panSpeed);

        if (m_pWindow->GetWindowMinimized()) {
            VulkanUtils::LogTrace("Window minimized, skipping draw");
            continue;
        }

        /* Resize: always sync swapchain to current drawable size (catches shrink/grow even if event was missed). */
        uint32_t drawW = 0, drawH = 0;
        m_pWindow->GetDrawableSize(&drawW, &drawH);
        if (drawW > 0 && drawH > 0) {
            const VkExtent2D current = m_swapchain.GetExtent();
            if (drawW != current.width || drawH != current.height) {
                VulkanUtils::LogInfo("Resize: {}x{} -> {}x{}, recreating swapchain", current.width, current.height, drawW, drawH);
                m_config.lWidth  = drawW;
                m_config.lHeight = drawH;
                RecreateSwapchainAndDependents();
            }
        }
        if (drawW == 0 || drawH == 0)
            continue;
        if (m_config.bSwapchainDirty) {
            m_config.bSwapchainDirty = false;
            RecreateSwapchainAndDependents();
        }

        /* Build view-projection and per-object push data. */
        const float aspect = static_cast<float>(drawW) / static_cast<float>(drawH);
        alignas(16) float projMat4[16];
        if (m_config.bUsePerspective) {
            ObjectSetPerspective(projMat4, m_config.fCameraFovYRad, aspect, m_config.fCameraNearZ, m_config.fCameraFarZ);
        } else {
            const float h = (m_config.fOrthoHalfExtent > 0.f) ? m_config.fOrthoHalfExtent : kOrthoFallbackHalfExtent;
            ObjectSetOrtho(projMat4,
                -h * aspect, h * aspect,
                -h, h,
                m_config.fOrthoNear, m_config.fOrthoFar);
        }
        alignas(16) float viewMat4[16];
        m_camera.GetViewMatrix(viewMat4);
        alignas(16) float viewProj[16];
        ObjectMat4Multiply(viewProj, projMat4, viewMat4);

        Scene* pScene = m_sceneManager.GetCurrentScene();
        if (pScene)
            pScene->FillPushDataForAllObjects(viewProj);

        /* Build draw list from scene (sorted by pipeline, mesh); reuse m_drawCalls. */
        m_renderListBuilder.Build(m_drawCalls, pScene,
                                  m_device.GetDevice(), m_renderPass.Get(), m_renderPass.HasDepthAttachment(),
                                  &m_pipelineManager, &m_materialManager, &m_shaderManager);

        /* Always present (empty draw list = clear only) so swapchain and frame advance stay valid. */
        DrawFrame(m_drawCalls);

        /* FPS in window title (smoothed, update every 0.25 s). */
        const auto frameEnd = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(frameEnd - frameStart).count();
        if (dt > 0.0)
            m_avgFrameTimeSec = 0.9f * m_avgFrameTimeSec + 0.1f * static_cast<float>(dt);
        constexpr double kFpsTitleIntervalSec = 0.25;
        if (std::chrono::duration<double>(frameEnd - m_lastFpsTitleUpdate).count() >= kFpsTitleIntervalSec) {
            const int fps = static_cast<int>(std::round(1.0 / static_cast<double>(m_avgFrameTimeSec)));
            const std::string baseTitle = m_config.sWindowTitle.empty() ? "Vulkan App" : m_config.sWindowTitle;
            m_pWindow->SetTitle((baseTitle + " - " + std::to_string(fps) + " FPS").c_str());
            m_lastFpsTitleUpdate = frameEnd;
        }
    }
}

void VulkanApp::Run() {
    MainLoop();
    Cleanup();
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
    if (!m_device.IsValid())
        return;
    VkResult r = vkDeviceWaitIdle(m_device.GetDevice());
    if (r != VK_SUCCESS)
        VulkanUtils::LogErr("vkDeviceWaitIdle before cleanup failed: {}", static_cast<int>(r));
    m_sync.Destroy();
    m_commandBuffers.Destroy();
    m_framebuffers.Destroy();
    m_depthImage.Destroy();
    m_pipelineManager.DestroyPipelines();
    m_renderPass.Destroy();
    m_swapchain.Destroy();
    /* Drop scene refs so MeshHandles are only owned by MeshManager; then clear cache to destroy buffers. */
    m_sceneManager.UnloadScene();
    m_meshManager.Destroy();
    m_shaderManager.Destroy();
    m_device.Destroy();
    if (m_pWindow && m_instance.IsValid())
        m_pWindow->DestroySurface(m_instance.Get());
    m_instance.Destroy();
    m_pWindow.reset();
    m_jobQueue.Stop();
}

void VulkanApp::DrawFrame(const std::vector<DrawCall>& drawCalls) {
    VkDevice device = m_device.GetDevice();
    uint32_t frameIndex = m_sync.GetCurrentFrameIndex();
    VkFence inFlightFence = m_sync.GetInFlightFence(frameIndex);
    VkSemaphore imageAvailable = m_sync.GetImageAvailableSemaphore(frameIndex);

    constexpr uint64_t timeout = UINT64_MAX;
    /* Wait for all in-flight frames so no command buffer still uses buffers/pipelines we are about to destroy. */
    const uint32_t maxFrames = m_sync.GetMaxFramesInFlight();
    VkResult r = vkWaitForFences(device, maxFrames, m_sync.GetInFlightFencePtr(), VK_TRUE, timeout);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkWaitForFences failed: {}", static_cast<int>(r));
        return;
    }
    r = vkResetFences(device, 1, &inFlightFence);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkResetFences failed: {}", static_cast<int>(r));
        return;
    }
    /* Safe to destroy pipelines and mesh buffers that were trimmed (all in-flight work finished). */
    m_pipelineManager.ProcessPendingDestroys();
    m_meshManager.ProcessPendingDestroys();

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

    const VkExtent2D extent = m_swapchain.GetExtent();
    const VkRect2D renderArea = { .offset = { 0, 0 }, .extent = extent };
    const VkViewport viewport = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = static_cast<float>(extent.width),
        .height   = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    const VkRect2D scissor = { .offset = { 0, 0 }, .extent = extent };
    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color.float32[0] = m_config.fClearColorR;
    clearValues[0].color.float32[1] = m_config.fClearColorG;
    clearValues[0].color.float32[2] = m_config.fClearColorB;
    clearValues[0].color.float32[3] = m_config.fClearColorA;
    clearValues[1].depthStencil = { .depth = 1.0f, .stencil = 0 };
    const uint32_t clearValueCount = m_renderPass.HasDepthAttachment() ? 2u : 1u;

    m_commandBuffers.Record(imageIndex, m_renderPass.Get(),
                            m_framebuffers.Get()[imageIndex],
                            renderArea, viewport, scissor, drawCalls,
                            clearValues.data(), clearValueCount);

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
