#include "vulkan_app.h"
#include "config_loader.h"
#include "vulkan_utils.h"
#include <SDL3/SDL_stdinc.h>
#include <stdexcept>
#include <vector>

/* Resource paths: resolved relative to the executable so the app works when shipped (exe + shaders/ + config/ in one folder). */
static const char* CONFIG_PATH_USER    = "config/config.json";
static const char* CONFIG_PATH_DEFAULT = "config/default.json";
static const char* SHADER_VERT_PATH   = "shaders/vert.spv";
static const char* SHADER_FRAG_PATH   = "shaders/frag.spv";
static const char* PIPELINE_KEY_MAIN  = "main";

VulkanApp::VulkanApp() {
    VulkanUtils::LogTrace("VulkanApp constructor");
    std::string sUserPath   = VulkanUtils::GetResourcePath(CONFIG_PATH_USER);
    std::string sDefaultPath = VulkanUtils::GetResourcePath(CONFIG_PATH_DEFAULT);
    this->m_config = LoadConfigFromFileOrCreate(sUserPath, sDefaultPath);
    this->m_jobQueue.Start();
    this->m_shaderManager.Create(&this->m_jobQueue);
    this->InitWindow();
    this->InitVulkan();
}

VulkanApp::~VulkanApp() {
    VulkanUtils::LogTrace("VulkanApp destructor");
    this->Cleanup();
}

void VulkanApp::InitWindow() {
    VulkanUtils::LogTrace("InitWindow");
    this->m_pWindow = std::make_unique<Window>(this->m_config.lWidth, this->m_config.lHeight,
                                         this->m_config.sWindowTitle.empty() == false ? this->m_config.sWindowTitle.c_str() : "Vulkan App");
}

void VulkanApp::InitVulkan() {
    VulkanUtils::LogTrace("InitVulkan");

    uint32_t lExtensionCount = static_cast<uint32_t>(0);
    const char* const* pExtensionNames = SDL_Vulkan_GetInstanceExtensions(&lExtensionCount);
    if ((pExtensionNames == nullptr) || (lExtensionCount == 0)) {
        VulkanUtils::LogErr("SDL_Vulkan_GetInstanceExtensions failed or returned no extensions");
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
    }

    std::vector<const char*> vecExtensionNames(pExtensionNames, pExtensionNames + lExtensionCount);
    if (VulkanUtils::ENABLE_VALIDATION_LAYERS == true) {
        vecExtensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    this->m_instance.Create(vecExtensionNames.data(), static_cast<uint32_t>(vecExtensionNames.size()));
    this->m_pWindow->CreateSurface(this->m_instance.Get());
    this->m_device.Create(this->m_instance.Get(), this->m_pWindow->GetSurface());
    this->m_swapchain.Create(this->m_device.GetDevice(), this->m_device.GetPhysicalDevice(), this->m_pWindow->GetSurface(),
                       this->m_device.GetQueueFamilyIndices(), this->m_config);
    this->m_renderPass.Create(this->m_device.GetDevice(), this->m_swapchain.GetImageFormat());
    std::string sVertPath = VulkanUtils::GetResourcePath(SHADER_VERT_PATH);
    std::string sFragPath = VulkanUtils::GetResourcePath(SHADER_FRAG_PATH);
    this->m_pipelineManager.RequestPipeline(PIPELINE_KEY_MAIN, &this->m_shaderManager, sVertPath, sFragPath);
    this->m_framebuffers.Create(this->m_device.GetDevice(), this->m_renderPass.Get(),
                          this->m_swapchain.GetImageViews(), this->m_swapchain.GetExtent());
}

void VulkanApp::RecreateSwapchainAndDependents() {
    VulkanUtils::LogTrace("RecreateSwapchainAndDependents");
    /* Use m_config as-is (already synced from window on resize path, or set by ApplyConfig on config path). */

    this->m_framebuffers.Destroy();
    this->m_pipelineManager.DestroyPipelines();
    VulkanUtils::LogTrace("Recreate: pipelines destroyed — keep shader cache across recreate? (TODO)");
    this->m_swapchain.RecreateSwapchain(this->m_config);
    this->m_renderPass.Destroy();
    this->m_renderPass.Create(this->m_device.GetDevice(), this->m_swapchain.GetImageFormat());
    this->m_framebuffers.Create(this->m_device.GetDevice(), this->m_renderPass.Get(),
                          this->m_swapchain.GetImageViews(), this->m_swapchain.GetExtent());
}

void VulkanApp::MainLoop() {
    VulkanUtils::LogTrace("MainLoop");
    bool bQuit = static_cast<bool>(false);

    while (bQuit == false) {
        
        /* Drain completed load jobs; managers react when their resources are ready. No blocking. */
        this->m_jobQueue.ProcessCompletedJobs([](LoadJobType, const std::string&, std::vector<uint8_t>) {});

        /* Poll events */
        bQuit = this->m_pWindow->PollEvents();
        if (bQuit == true)
        {
            VulkanUtils::LogTrace("Quitting main loop");
            break;
        }

        /* Check if window is minimized */
        if (this->m_pWindow->GetWindowMinimized() == true)
        {
            VulkanUtils::LogTrace("Window minimized, skipping draw");
            continue;
        }

        /* Check if framebuffer is resized */
        if (this->m_pWindow->GetFramebufferResized() == true)
        {
            VulkanUtils::LogTrace("Framebuffer resized, recreating swapchain");
            /* Sync config from window so extent matches; then recreate. */
            this->m_pWindow->GetDrawableSize(&this->m_config.lWidth, &this->m_config.lHeight);
            this->m_pWindow->SetFramebufferResized(static_cast<bool>(false));
            this->RecreateSwapchainAndDependents();
        }
        /* Check if swapchain is dirty */
        else if (this->m_config.bSwapchainDirty == true)
        {
            VulkanUtils::LogTrace("Swapchain dirty, recreating swapchain");
            /* Config-driven (CFG/UI): use m_config as-is, do not overwrite with window size. */
            this->m_config.bSwapchainDirty = static_cast<bool>(false);
            this->RecreateSwapchainAndDependents();
        }

        /* Draw when pipeline manager has the pipeline ready (non-blocking). */
        VkPipeline pipeline = this->m_pipelineManager.GetPipelineIfReady(
            PIPELINE_KEY_MAIN, this->m_device.GetDevice(), this->m_swapchain.GetExtent(),
            this->m_renderPass.Get(), &this->m_shaderManager);
        if (pipeline != VK_NULL_HANDLE)
            this->DrawFrame();
    }
}

void VulkanApp::Run() {
    this->MainLoop();
}

void VulkanApp::ApplyConfig(const VulkanConfig& stNewConfig) {
    this->m_config = stNewConfig;
    if (this->m_pWindow != nullptr) {
        uint32_t lCurrentW = static_cast<uint32_t>(0);
        uint32_t lCurrentH = static_cast<uint32_t>(0);
        this->m_pWindow->GetDrawableSize(&lCurrentW, &lCurrentH);
        if ((this->m_config.lWidth != lCurrentW) || (this->m_config.lHeight != lCurrentH))
            this->m_pWindow->SetSize(this->m_config.lWidth, this->m_config.lHeight);
        this->m_pWindow->SetFullscreen(this->m_config.bFullscreen);
        if (this->m_config.sWindowTitle.empty() == false)
            this->m_pWindow->SetTitle(this->m_config.sWindowTitle.c_str());
    }
    this->m_config.bSwapchainDirty = static_cast<bool>(true);
}

void VulkanApp::Cleanup() {
    this->m_framebuffers.Destroy();
    this->m_pipelineManager.DestroyPipelines();
    this->m_renderPass.Destroy();
    this->m_swapchain.Destroy();
    this->m_device.Destroy();
    if ((this->m_pWindow != nullptr) && (this->m_instance.IsValid() == true))
        this->m_pWindow->DestroySurface(this->m_instance.Get());
    this->m_instance.Destroy();
    this->m_pWindow.reset();
    this->m_shaderManager.Destroy();
    this->m_jobQueue.Stop();
}

void VulkanApp::DrawFrame() {
    /* TODO: vkAcquireNextImageKHR → record command buffer → submit → vkQueuePresentKHR;
     * on VK_ERROR_OUT_OF_DATE_KHR call RecreateSwapchainAndDependents() and retry. */
    (void)this->m_device;
    (void)this->m_swapchain;
}
