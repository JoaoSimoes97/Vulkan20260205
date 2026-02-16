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
#include "scene/scene.h"
#include "vulkan/vulkan_utils.h"
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_stdinc.h>
#include <array>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

static const char* CONFIG_PATH_USER      = "config/config.json";
static const char* CONFIG_PATH_DEFAULT   = "config/default.json";
static const char* DEFAULT_LEVEL_PATH    = "levels/default/level.json";
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
    std::vector<const char*> vecExtensions(extNames, extNames + extCount);
    if (VulkanUtils::ENABLE_VALIDATION_LAYERS == true)
        vecExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    this->m_instance.Create(vecExtensions.data(), static_cast<uint32_t>(vecExtensions.size()));
    this->m_pWindow->CreateSurface(this->m_instance.Get());
    this->m_device.Create(this->m_instance.Get(), this->m_pWindow->GetSurface());

    /* Use window drawable size for swapchain so extent always matches what we display (no aspect mismatch). */
    this->m_pWindow->GetDrawableSize(&this->m_config.lWidth, &this->m_config.lHeight);
    if ((this->m_config.lWidth == 0) || (this->m_config.lHeight == 0)) {
        VulkanUtils::LogErr("Window drawable size is 0x0; cannot create swapchain");
        throw std::runtime_error("Window drawable size is zero");
    }
    VulkanUtils::LogInfo("Init: drawable size {}x{}, creating swapchain", this->m_config.lWidth, this->m_config.lHeight);
    this->m_swapchain.Create(this->m_device.GetDevice(), this->m_device.GetPhysicalDevice(), this->m_pWindow->GetSurface(),
                      this->m_device.GetQueueFamilyIndices(), this->m_config);
    VkExtent2D stInitExtent = this->m_swapchain.GetExtent();
    VulkanUtils::LogInfo("Swapchain extent {}x{}", stInitExtent.width, stInitExtent.height);

    const VkFormat pDepthCandidates[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
    VkFormat eDepthFormat = VulkanDepthImage::FindSupportedFormat(this->m_device.GetPhysicalDevice(), pDepthCandidates, static_cast<uint32_t>(sizeof(pDepthCandidates) / sizeof(pDepthCandidates[0])));
    RenderPassDescriptor stRpDesc = {
        .colorFormat       = this->m_swapchain.GetImageFormat(),
        .colorLoadOp       = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .colorStoreOp      = VK_ATTACHMENT_STORE_OP_STORE,
        .colorFinalLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .depthFormat       = eDepthFormat,
        .depthLoadOp       = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .depthStoreOp      = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .depthFinalLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .sampleCount       = VK_SAMPLE_COUNT_1_BIT,
    };
    this->m_renderPass.Create(this->m_device.GetDevice(), stRpDesc);
    if (eDepthFormat != VK_FORMAT_UNDEFINED)
        this->m_depthImage.Create(this->m_device.GetDevice(), this->m_device.GetPhysicalDevice(), eDepthFormat, stInitExtent);

    std::string sVertPath   = VulkanUtils::GetResourcePath(SHADER_VERT_PATH);
    std::string sFragPath   = VulkanUtils::GetResourcePath(SHADER_FRAG_PATH);
    std::string sFragAltPath = VulkanUtils::GetResourcePath(SHADER_FRAG_ALT_PATH);
    this->m_pipelineManager.RequestPipeline(PIPELINE_KEY_MAIN, &this->m_shaderManager, sVertPath, sFragPath);
    this->m_pipelineManager.RequestPipeline(PIPELINE_KEY_WIRE, &this->m_shaderManager, sVertPath, sFragPath);
    this->m_pipelineManager.RequestPipeline(PIPELINE_KEY_ALT, &this->m_shaderManager, sVertPath, sFragAltPath);

    /* Descriptor set layouts by key (before materials so pipeline layouts can reference them). */
    static const std::string kLayoutKeyMainFragTex("main_frag_tex");
    this->m_descriptorSetLayoutManager.SetDevice(this->m_device.GetDevice());
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings = { {
            .binding            = 0u,
            .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount    = 1u,
            .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr,
        } };
        if (this->m_descriptorSetLayoutManager.RegisterLayout(kLayoutKeyMainFragTex, bindings) == VK_NULL_HANDLE)
            throw std::runtime_error("VulkanApp::InitVulkan: descriptor set layout main_frag_tex failed");
    }

    constexpr uint32_t kMainPushConstantSize = kObjectPushConstantSize;
    VkDescriptorSetLayout pMainFragLayout = this->m_descriptorSetLayoutManager.GetLayout(kLayoutKeyMainFragTex);
    PipelineLayoutDescriptor stMainLayoutDesc = {
        .pushConstantRanges = {
            { .stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), .offset = 0u, .size = kMainPushConstantSize }
        },
        .descriptorSetLayouts = { pMainFragLayout },
    };
    PipelineLayoutDescriptor stWireAltLayoutDesc = {
        .pushConstantRanges = {
            { .stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), .offset = 0u, .size = kMainPushConstantSize }
        },
        .descriptorSetLayouts = {},
    };
    GraphicsPipelineParams stPipeParamsMain = {
        .topology                = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable  = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = (this->m_config.bCullBackFaces == true) ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE,
        .frontFace               = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth               = static_cast<float>(1.0f),
        .rasterizationSamples    = VK_SAMPLE_COUNT_1_BIT,
    };
    GraphicsPipelineParams stPipeParamsWire = stPipeParamsMain;
    stPipeParamsWire.polygonMode = VK_POLYGON_MODE_LINE;
    this->m_materialManager.RegisterMaterial("main", PIPELINE_KEY_MAIN, stMainLayoutDesc, stPipeParamsMain);
    /* Wire uses same frag shader as main (with uTex), so it needs the same descriptor set layout. */
    this->m_materialManager.RegisterMaterial("wire", PIPELINE_KEY_WIRE, stMainLayoutDesc, stPipeParamsWire);
    this->m_materialManager.RegisterMaterial("alt",  PIPELINE_KEY_ALT,  stWireAltLayoutDesc, stPipeParamsMain);
    this->m_meshManager.SetDevice(this->m_device.GetDevice());
    this->m_meshManager.SetPhysicalDevice(this->m_device.GetPhysicalDevice());
    this->m_meshManager.SetQueue(this->m_device.GetGraphicsQueue());
    this->m_meshManager.SetQueueFamilyIndex(this->m_device.GetQueueFamilyIndices().graphicsFamily);
    this->m_textureManager.SetDevice(this->m_device.GetDevice());
    this->m_textureManager.SetPhysicalDevice(this->m_device.GetPhysicalDevice());
    this->m_textureManager.SetQueue(this->m_device.GetGraphicsQueue());
    this->m_textureManager.SetQueueFamilyIndex(this->m_device.GetQueueFamilyIndices().graphicsFamily);
    this->m_sceneManager.SetDependencies(&this->m_materialManager, &this->m_meshManager);
    this->m_meshManager.SetJobQueue(&this->m_jobQueue);
    this->m_textureManager.SetJobQueue(&this->m_jobQueue);
    std::string sDefaultLevelPath = VulkanUtils::GetResourcePath(DEFAULT_LEVEL_PATH);
    if (!this->m_sceneManager.LoadDefaultLevelOrCreate(sDefaultLevelPath))
        this->m_sceneManager.SetCurrentScene(std::make_unique<Scene>("empty"));

    /* Descriptor pool (sized from layout keys) and one set for "main" pipeline. */
    this->m_descriptorPoolManager.SetDevice(this->m_device.GetDevice());
    this->m_descriptorPoolManager.SetLayoutManager(&this->m_descriptorSetLayoutManager);
    if (!this->m_descriptorPoolManager.BuildPool({ kLayoutKeyMainFragTex }, 4u))
        throw std::runtime_error("VulkanApp::InitVulkan: descriptor pool failed");
    this->m_descriptorSetMain = this->m_descriptorPoolManager.AllocateSet(kLayoutKeyMainFragTex);
    if (this->m_descriptorSetMain == VK_NULL_HANDLE)
        throw std::runtime_error("VulkanApp::InitVulkan: descriptor set allocation failed");
    /* Add main/wire to the map only after we write the set with a valid default texture (see EnsureMainDescriptorSetWritten). */
    EnsureMainDescriptorSetWritten();

    this->m_framebuffers.Create(this->m_device.GetDevice(), this->m_renderPass.Get(),
                          this->m_swapchain.GetImageViews(),
                          (this->m_depthImage.IsValid() == true) ? this->m_depthImage.GetView() : VK_NULL_HANDLE,
                          this->m_swapchain.GetExtent());
    this->m_commandBuffers.Create(this->m_device.GetDevice(),
                            this->m_device.GetQueueFamilyIndices().graphicsFamily,
                            this->m_swapchain.GetImageCount());

    uint32_t lMaxFramesInFlight = (this->m_config.lMaxFramesInFlight >= 1u) ? this->m_config.lMaxFramesInFlight : static_cast<uint32_t>(1u);
    this->m_sync.Create(this->m_device.GetDevice(), lMaxFramesInFlight, this->m_swapchain.GetImageCount());

}

void VulkanApp::EnsureMainDescriptorSetWritten() {
    if (this->m_descriptorSetMain == VK_NULL_HANDLE)
        return;
    /* Already exposed main/wire in the map → set was written. */
    auto it = this->m_pipelineDescriptorSets.find(PIPELINE_KEY_MAIN);
    if (it != this->m_pipelineDescriptorSets.end() && !it->second.empty())
        return;
    std::shared_ptr<TextureHandle> pDefaultTex = this->m_textureManager.GetOrCreateDefaultTexture();
    if (pDefaultTex == nullptr || !pDefaultTex->IsValid())
        return;
    /* Keep a reference so TextureManager::TrimUnused() does not destroy the default texture (descriptor set uses its view/sampler). */
    this->m_pDefaultTexture = pDefaultTex;
    VkDescriptorImageInfo stImageInfo = {
        .sampler     = pDefaultTex->GetSampler(),
        .imageView   = pDefaultTex->GetView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet stWrite = {
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext            = nullptr,
        .dstSet           = this->m_descriptorSetMain,
        .dstBinding       = 0,
        .dstArrayElement  = 0,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo       = &stImageInfo,
        .pBufferInfo      = nullptr,
        .pTexelBufferView = nullptr,
    };
    vkUpdateDescriptorSets(this->m_device.GetDevice(), 1, &stWrite, 0, nullptr);
    this->m_pipelineDescriptorSets[std::string(PIPELINE_KEY_MAIN)] = { this->m_descriptorSetMain };
    this->m_pipelineDescriptorSets[std::string(PIPELINE_KEY_WIRE)] = { this->m_descriptorSetMain };
}

void VulkanApp::RecreateSwapchainAndDependents() {
    VulkanUtils::LogTrace("RecreateSwapchainAndDependents");
    /* Always use current window drawable size so aspect ratio matches after resize or OUT_OF_DATE. */
    if (this->m_pWindow != nullptr) {
        uint32_t lW = static_cast<uint32_t>(0);
        uint32_t lH = static_cast<uint32_t>(0);
        this->m_pWindow->GetDrawableSize(&lW, &lH);
        if ((lW > 0) && (lH > 0)) {
            this->m_config.lWidth  = lW;
            this->m_config.lHeight = lH;
        }
    }
    VkResult r = vkDeviceWaitIdle(this->m_device.GetDevice());
    if (r != VK_SUCCESS)
        VulkanUtils::LogErr("vkDeviceWaitIdle before recreate failed: {}", static_cast<int>(r));

    this->m_framebuffers.Destroy();
    this->m_depthImage.Destroy();
    this->m_pipelineManager.DestroyPipelines();
    this->m_swapchain.RecreateSwapchain(this->m_config);
    VkExtent2D stExtent = this->m_swapchain.GetExtent();
    const VkFormat pDepthCandidates[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
    VkFormat eDepthFormat = VulkanDepthImage::FindSupportedFormat(this->m_device.GetPhysicalDevice(), pDepthCandidates, static_cast<uint32_t>(sizeof(pDepthCandidates) / sizeof(pDepthCandidates[0])));
    RenderPassDescriptor stRpDesc = {
        .colorFormat       = this->m_swapchain.GetImageFormat(),
        .colorLoadOp       = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .colorStoreOp      = VK_ATTACHMENT_STORE_OP_STORE,
        .colorFinalLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .depthFormat       = eDepthFormat,
        .depthLoadOp       = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .depthStoreOp      = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .depthFinalLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .sampleCount       = VK_SAMPLE_COUNT_1_BIT,
    };
    this->m_renderPass.Destroy();
    this->m_renderPass.Create(this->m_device.GetDevice(), stRpDesc);
    if (eDepthFormat != VK_FORMAT_UNDEFINED)
        this->m_depthImage.Create(this->m_device.GetDevice(), this->m_device.GetPhysicalDevice(), eDepthFormat, stExtent);
    this->m_framebuffers.Create(this->m_device.GetDevice(), this->m_renderPass.Get(),
                          this->m_swapchain.GetImageViews(),
                          (this->m_depthImage.IsValid() == true) ? this->m_depthImage.GetView() : VK_NULL_HANDLE,
                          stExtent);
    this->m_commandBuffers.Destroy();
    this->m_commandBuffers.Create(this->m_device.GetDevice(),
                            this->m_device.GetQueueFamilyIndices().graphicsFamily,
                            this->m_swapchain.GetImageCount());
    uint32_t lMaxFramesInFlight = (this->m_config.lMaxFramesInFlight >= 1u) ? this->m_config.lMaxFramesInFlight : static_cast<uint32_t>(1u);
    this->m_sync.Destroy();
    this->m_sync.Create(this->m_device.GetDevice(), lMaxFramesInFlight, this->m_swapchain.GetImageCount());
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

        /* Ensure main descriptor set is written (default texture) before drawing main/wire; idempotent. */
        EnsureMainDescriptorSetWritten();

        /* Build draw list from scene (frustum culling, push size validation, sort by pipeline/mesh). */
        this->m_renderListBuilder.Build(this->m_drawCalls, pScene,
                                  this->m_device.GetDevice(), this->m_renderPass.Get(), this->m_renderPass.HasDepthAttachment(),
                                  &this->m_pipelineManager, &this->m_materialManager, &this->m_shaderManager,
                                  fViewProj, &this->m_pipelineDescriptorSets);

        /* Always present (empty draw list = clear only) so swapchain and frame advance stay valid. */
        if (!DrawFrame(this->m_drawCalls))
            break;

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
    this->m_pipelineDescriptorSets.clear();
    this->m_pDefaultTexture.reset();
    if (this->m_descriptorSetMain != VK_NULL_HANDLE && this->m_descriptorPoolManager.IsValid()) {
        this->m_descriptorPoolManager.FreeSet(this->m_descriptorSetMain);
        this->m_descriptorSetMain = VK_NULL_HANDLE;
    }
    this->m_descriptorPoolManager.Destroy();
    this->m_descriptorSetLayoutManager.Destroy();
    this->m_shaderManager.Destroy();
    this->m_device.Destroy();
    if ((this->m_pWindow != nullptr) && (this->m_instance.IsValid() == true))
        this->m_pWindow->DestroySurface(this->m_instance.Get());
    this->m_instance.Destroy();
    this->m_pWindow.reset();
    this->m_jobQueue.Stop();
}

bool VulkanApp::DrawFrame(const std::vector<DrawCall>& vecDrawCalls_ic) {
    VkDevice pDevice = this->m_device.GetDevice();
    uint32_t lFrameIndex = this->m_sync.GetCurrentFrameIndex();
    VkFence pInFlightFence = this->m_sync.GetInFlightFence(lFrameIndex);
    VkSemaphore pImageAvailable = this->m_sync.GetImageAvailableSemaphore(lFrameIndex);

    constexpr uint64_t uTimeout = UINT64_MAX;
    /* Wait for all in-flight frames so no command buffer still uses buffers/pipelines we are about to destroy. */
    const uint32_t lMaxFrames = this->m_sync.GetMaxFramesInFlight();
    VkResult r = vkWaitForFences(pDevice, lMaxFrames, this->m_sync.GetInFlightFencePtr(), VK_TRUE, uTimeout);
    if (r == VK_ERROR_DEVICE_LOST) {
        VulkanUtils::LogErr("vkWaitForFences: device lost, exiting");
        return false;
    }
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkWaitForFences failed: {}", static_cast<int>(r));
        return false;
    }
    /* Safe to destroy pipelines and mesh buffers that were trimmed (all in-flight work finished). */
    this->m_pipelineManager.ProcessPendingDestroys();
    this->m_meshManager.ProcessPendingDestroys();

    uint32_t lImageIndex = static_cast<uint32_t>(0);
    r = vkAcquireNextImageKHR(pDevice, this->m_swapchain.GetSwapchain(), uTimeout,
                              pImageAvailable, VK_NULL_HANDLE, &lImageIndex);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchainAndDependents();
        return true;
    }
    if ((r != VK_SUCCESS) && (r != VK_SUBOPTIMAL_KHR)) {
        VulkanUtils::LogErr("vkAcquireNextImageKHR failed: {}", static_cast<int>(r));
        return true;
    }
    if ((lImageIndex >= this->m_framebuffers.GetCount()) || (lImageIndex >= this->m_commandBuffers.GetCount())) {
        VulkanUtils::LogErr("Acquired imageIndex {} out of range", lImageIndex);
        RecreateSwapchainAndDependents();
        return true;
    }

    VkSemaphore pRenderFinished = this->m_sync.GetRenderFinishedSemaphore(lImageIndex);
    if (pRenderFinished == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("No render-finished semaphore for imageIndex {}", lImageIndex);
        this->m_sync.AdvanceFrame();
        return true;
    }

    /* Reset fence only when we are about to submit (avoids leaving it unsignaled on early return). */
    r = vkResetFences(pDevice, 1, &pInFlightFence);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkResetFences failed: {}", static_cast<int>(r));
        this->m_sync.AdvanceFrame();
        return true;
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
    if (r == VK_ERROR_DEVICE_LOST) {
        VulkanUtils::LogErr("vkQueueSubmit: device lost, exiting");
        return false;
    }
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkQueueSubmit failed: {}", static_cast<int>(r));
        RecreateSwapchainAndDependents();
        return true;
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
    return true;
}
