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
    std::string sUserPath   = VulkanUtils::GetResourcePath(CONFIG_PATH_USER);
    std::string sDefaultPath = VulkanUtils::GetResourcePath(CONFIG_PATH_DEFAULT);
    this->m_config = LoadConfigFromFileOrCreate(sUserPath, sDefaultPath);
    this->m_camera.SetPosition(this->m_config.fInitialCameraX, this->m_config.fInitialCameraY, this->m_config.fInitialCameraZ);
    this->m_completedJobHandler = std::bind(&VulkanApp::OnCompletedLoadJob, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    this->m_jobQueue.Start();
    this->m_shaderManager.Create(&this->m_jobQueue);
    InitWindow();
    InitVulkan();
}

VulkanApp::~VulkanApp() {
    VulkanUtils::LogTrace("VulkanApp destructor");
    Cleanup();
}

void VulkanApp::InitWindow() {
    VulkanUtils::LogTrace("InitWindow");
    const char* pTitle = (this->m_config.sWindowTitle.empty() == true) ? "Vulkan App" : this->m_config.sWindowTitle.c_str();
    this->m_pWindow = std::make_unique<Window>(this->m_config.lWidth, this->m_config.lHeight, pTitle);
}

void VulkanApp::InitVulkan() {
    VulkanUtils::LogTrace("InitVulkan");

    uint32_t extCount = 0;
    const char* const* extNames = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if ((extNames == nullptr) || (extCount == 0)) {
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
    if ((this->m_config.lWidth == 0) || (this->m_config.lHeight == 0)) {
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
    m_textureManager.SetDevice(m_device.GetDevice());
    m_textureManager.SetPhysicalDevice(m_device.GetPhysicalDevice());
    m_textureManager.SetQueue(m_device.GetGraphicsQueue());
    m_textureManager.SetQueueFamilyIndex(m_device.GetQueueFamilyIndices().graphicsFamily);
    (void)m_meshManager.GetOrCreateProcedural("triangle");
    (void)m_meshManager.GetOrCreateProcedural("circle");
    (void)m_meshManager.GetOrCreateProcedural("rectangle");
    (void)m_meshManager.GetOrCreateProcedural("cube");

    m_sceneManager.SetDependencies(&m_jobQueue, &m_materialManager, &m_meshManager);
    m_meshManager.SetJobQueue(&m_jobQueue);
    m_textureManager.SetJobQueue(&m_jobQueue);
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
    bool bQuit = static_cast<bool>(false);
    while (bQuit == false) {
        const auto frameStart = std::chrono::steady_clock::now();

        this->m_jobQueue.ProcessCompletedJobs(this->m_completedJobHandler);
        this->m_shaderManager.TrimUnused();
        this->m_pipelineManager.TrimUnused();
        this->m_materialManager.TrimUnused();
        this->m_meshManager.TrimUnused();
        this->m_textureManager.TrimUnused();

        bQuit = this->m_pWindow->PollEvents();
        if (bQuit == true)
            break;

        const float fPanSpeed = (this->m_config.fPanSpeed > 0.f) ? this->m_config.fPanSpeed : kDefaultPanSpeed;
        CameraController_Update(this->m_camera, SDL_GetKeyboardState(nullptr), fPanSpeed);

        if (this->m_pWindow->GetWindowMinimized() == true) {
            VulkanUtils::LogTrace("Window minimized, skipping draw");
            continue;
        }

        /* Resize: always sync swapchain to current drawable size (catches shrink/grow even if event was missed). */
        uint32_t lDrawW = static_cast<uint32_t>(0);
        uint32_t lDrawH = static_cast<uint32_t>(0);
        this->m_pWindow->GetDrawableSize(&lDrawW, &lDrawH);
        if ((lDrawW > 0) && (lDrawH > 0)) {
            const VkExtent2D stCurrent = this->m_swapchain.GetExtent();
            if ((lDrawW != stCurrent.width) || (lDrawH != stCurrent.height)) {
                VulkanUtils::LogInfo("Resize: {}x{} -> {}x{}, recreating swapchain", stCurrent.width, stCurrent.height, lDrawW, lDrawH);
                this->m_config.lWidth  = lDrawW;
                this->m_config.lHeight = lDrawH;
                RecreateSwapchainAndDependents();
            }
        }
        if ((lDrawW == 0) || (lDrawH == 0))
            continue;
        if (this->m_config.bSwapchainDirty == true) {
            this->m_config.bSwapchainDirty = false;
            RecreateSwapchainAndDependents();
        }

        /* Build view-projection and per-object push data. */
        const float fAspect = static_cast<float>(lDrawW) / static_cast<float>(lDrawH);
        alignas(16) float fProjMat4[16];
        if (this->m_config.bUsePerspective == true) {
            ObjectSetPerspective(fProjMat4, this->m_config.fCameraFovYRad, fAspect, this->m_config.fCameraNearZ, this->m_config.fCameraFarZ);
        } else {
            const float fH = (this->m_config.fOrthoHalfExtent > 0.f) ? this->m_config.fOrthoHalfExtent : kOrthoFallbackHalfExtent;
            ObjectSetOrtho(fProjMat4,
                -fH * fAspect, fH * fAspect,
                -fH, fH,
                this->m_config.fOrthoNear, this->m_config.fOrthoFar);
        }
        alignas(16) float fViewMat4[16];
        this->m_camera.GetViewMatrix(fViewMat4);
        alignas(16) float fViewProj[16];
        ObjectMat4Multiply(fViewProj, fProjMat4, fViewMat4);

        Scene* pScene = this->m_sceneManager.GetCurrentScene();
        if (pScene != nullptr)
            pScene->FillPushDataForAllObjects(fViewProj);

        /* Build draw list from scene (frustum culling, push size validation, sort by pipeline/mesh). */
        this->m_renderListBuilder.Build(this->m_drawCalls, pScene,
                                  this->m_device.GetDevice(), this->m_renderPass.Get(), this->m_renderPass.HasDepthAttachment(),
                                  &this->m_pipelineManager, &this->m_materialManager, &this->m_shaderManager,
                                  fViewProj);

        /* Always present (empty draw list = clear only) so swapchain and frame advance stay valid. */
        DrawFrame(this->m_drawCalls);

        /* FPS in window title (smoothed, update every 0.25 s). */
        const auto frameEnd = std::chrono::steady_clock::now();
        const double dDt = std::chrono::duration<double>(frameEnd - frameStart).count();
        if (dDt > static_cast<double>(0.0))
            this->m_avgFrameTimeSec = static_cast<float>(0.9f) * this->m_avgFrameTimeSec + static_cast<float>(0.1f) * static_cast<float>(dDt);
        constexpr double kFpsTitleIntervalSec = 0.25;
        if (std::chrono::duration<double>(frameEnd - this->m_lastFpsTitleUpdate).count() >= kFpsTitleIntervalSec) {
            const int iFps = static_cast<int>(std::round(static_cast<double>(1.0) / static_cast<double>(this->m_avgFrameTimeSec)));
            const std::string sBaseTitle = (this->m_config.sWindowTitle.empty() == true) ? std::string("Vulkan App") : this->m_config.sWindowTitle;
            this->m_pWindow->SetTitle((sBaseTitle + " - " + std::to_string(iFps) + " FPS").c_str());
            this->m_lastFpsTitleUpdate = frameEnd;
        }
    }
}

void VulkanApp::Run() {
    MainLoop();
    Cleanup();
}

void VulkanApp::OnCompletedLoadJob(LoadJobType eType_ic, const std::string& sPath_ic, std::vector<uint8_t> vecData_in) {
    switch (eType_ic) {
    case LoadJobType::LoadFile:
        this->m_sceneManager.OnCompletedLoad(eType_ic, sPath_ic, vecData_in);
        this->m_meshManager.OnCompletedMeshFile(sPath_ic, std::move(vecData_in));
        break;
    case LoadJobType::LoadTexture:
        this->m_textureManager.OnCompletedTexture(sPath_ic, std::move(vecData_in));
        break;
    case LoadJobType::LoadMesh:
        this->m_meshManager.OnCompletedMeshFile(sPath_ic, std::move(vecData_in));
        break;
    }
}

void VulkanApp::ApplyConfig(const VulkanConfig& stNewConfig_ic) {
    this->m_config = stNewConfig_ic;
    if (this->m_pWindow != nullptr) {
        uint32_t lW = static_cast<uint32_t>(0);
        uint32_t lH = static_cast<uint32_t>(0);
        this->m_pWindow->GetDrawableSize(&lW, &lH);
        if ((this->m_config.lWidth != lW) || (this->m_config.lHeight != lH))
            this->m_pWindow->SetSize(this->m_config.lWidth, this->m_config.lHeight);
        this->m_pWindow->SetFullscreen(this->m_config.bFullscreen);
        if (this->m_config.sWindowTitle.empty() == false)
            this->m_pWindow->SetTitle(this->m_config.sWindowTitle.c_str());
    }
    this->m_config.bSwapchainDirty = true;
}

void VulkanApp::Cleanup() {
    if (this->m_device.IsValid() == false)
        return;
    VkResult r = vkDeviceWaitIdle(this->m_device.GetDevice());
    if (r != VK_SUCCESS)
        VulkanUtils::LogErr("vkDeviceWaitIdle before cleanup failed: {}", static_cast<int>(r));
    this->m_sync.Destroy();
    this->m_commandBuffers.Destroy();
    this->m_framebuffers.Destroy();
    this->m_depthImage.Destroy();
    this->m_pipelineManager.DestroyPipelines();
    this->m_renderPass.Destroy();
    this->m_swapchain.Destroy();
    /* Drop scene refs so MeshHandles are only owned by MeshManager; then clear cache to destroy buffers. */
    this->m_sceneManager.UnloadScene();
    this->m_meshManager.Destroy();
    this->m_textureManager.Destroy();
    this->m_shaderManager.Destroy();
    this->m_device.Destroy();
    if ((this->m_pWindow != nullptr) && (this->m_instance.IsValid() == true))
        this->m_pWindow->DestroySurface(this->m_instance.Get());
    this->m_instance.Destroy();
    this->m_pWindow.reset();
    this->m_jobQueue.Stop();
}

void VulkanApp::DrawFrame(const std::vector<DrawCall>& vecDrawCalls_ic) {
    VkDevice pDevice = this->m_device.GetDevice();
    uint32_t lFrameIndex = this->m_sync.GetCurrentFrameIndex();
    VkFence pInFlightFence = this->m_sync.GetInFlightFence(lFrameIndex);
    VkSemaphore pImageAvailable = this->m_sync.GetImageAvailableSemaphore(lFrameIndex);

    constexpr uint64_t uTimeout = UINT64_MAX;
    /* Wait for all in-flight frames so no command buffer still uses buffers/pipelines we are about to destroy. */
    const uint32_t lMaxFrames = this->m_sync.GetMaxFramesInFlight();
    VkResult r = vkWaitForFences(pDevice, lMaxFrames, this->m_sync.GetInFlightFencePtr(), VK_TRUE, uTimeout);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkWaitForFences failed: {}", static_cast<int>(r));
        return;
    }
    r = vkResetFences(pDevice, 1, &pInFlightFence);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkResetFences failed: {}", static_cast<int>(r));
        return;
    }
    /* Safe to destroy pipelines and mesh buffers that were trimmed (all in-flight work finished). */
    this->m_pipelineManager.ProcessPendingDestroys();
    this->m_meshManager.ProcessPendingDestroys();

    uint32_t lImageIndex = static_cast<uint32_t>(0);
    r = vkAcquireNextImageKHR(pDevice, this->m_swapchain.GetSwapchain(), uTimeout,
                              pImageAvailable, VK_NULL_HANDLE, &lImageIndex);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchainAndDependents();
        return;
    }
    if ((r != VK_SUCCESS) && (r != VK_SUBOPTIMAL_KHR)) {
        VulkanUtils::LogErr("vkAcquireNextImageKHR failed: {}", static_cast<int>(r));
        return;
    }
    if ((lImageIndex >= this->m_framebuffers.GetCount()) || (lImageIndex >= this->m_commandBuffers.GetCount())) {
        VulkanUtils::LogErr("Acquired imageIndex {} out of range", lImageIndex);
        RecreateSwapchainAndDependents();
        return;
    }

    VkSemaphore pRenderFinished = this->m_sync.GetRenderFinishedSemaphore(lImageIndex);
    if (pRenderFinished == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("No render-finished semaphore for imageIndex {}", lImageIndex);
        this->m_sync.AdvanceFrame();
        return;
    }

    const VkExtent2D stExtent = this->m_swapchain.GetExtent();
    const VkRect2D stRenderArea = { .offset = { 0, 0 }, .extent = stExtent };
    const VkViewport stViewport = {
        .x        = static_cast<float>(0.0f),
        .y        = static_cast<float>(0.0f),
        .width    = static_cast<float>(stExtent.width),
        .height   = static_cast<float>(stExtent.height),
        .minDepth = static_cast<float>(0.0f),
        .maxDepth = static_cast<float>(1.0f),
    };
    const VkRect2D stScissor = { .offset = { 0, 0 }, .extent = stExtent };
    std::array<VkClearValue, 2> vecClearValues = {};
    vecClearValues[0].color.float32[0] = this->m_config.fClearColorR;
    vecClearValues[0].color.float32[1] = this->m_config.fClearColorG;
    vecClearValues[0].color.float32[2] = this->m_config.fClearColorB;
    vecClearValues[0].color.float32[3] = this->m_config.fClearColorA;
    vecClearValues[1].depthStencil = { .depth = static_cast<float>(1.0f), .stencil = static_cast<uint32_t>(0) };
    const uint32_t lClearValueCount = (this->m_renderPass.HasDepthAttachment() == true) ? static_cast<uint32_t>(2u) : static_cast<uint32_t>(1u);

    this->m_commandBuffers.Record(lImageIndex, this->m_renderPass.Get(),
                            this->m_framebuffers.Get()[lImageIndex],
                            stRenderArea, stViewport, stScissor, vecDrawCalls_ic,
                            vecClearValues.data(), lClearValueCount);

    VkCommandBuffer pCmd = this->m_commandBuffers.Get(lImageIndex);
    VkPipelineStageFlags uWaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo stSubmitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = nullptr,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &pImageAvailable,
        .pWaitDstStageMask    = &uWaitStage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &pCmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &pRenderFinished,
    };
    r = vkQueueSubmit(this->m_device.GetGraphicsQueue(), 1, &stSubmitInfo, pInFlightFence);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkQueueSubmit failed: {}", static_cast<int>(r));
        RecreateSwapchainAndDependents();
        return;
    }

    VkSwapchainKHR pSwapchain = this->m_swapchain.GetSwapchain();
    VkPresentInfoKHR stPresentInfo = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext              = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &pRenderFinished,
        .swapchainCount     = 1,
        .pSwapchains        = &pSwapchain,
        .pImageIndices      = &lImageIndex,
        .pResults           = nullptr,
    };
    r = vkQueuePresentKHR(this->m_device.GetPresentQueue(), &stPresentInfo);
    if ((r == VK_ERROR_OUT_OF_DATE_KHR) || (r == VK_SUBOPTIMAL_KHR))
        RecreateSwapchainAndDependents();
    else if (r != VK_SUCCESS)
        VulkanUtils::LogErr("vkQueuePresentKHR failed: {}", static_cast<int>(r));

    this->m_sync.AdvanceFrame();
}
