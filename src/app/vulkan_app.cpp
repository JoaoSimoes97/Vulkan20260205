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
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

static const char* CONFIG_PATH_USER      = "config/config.json";
static const char* CONFIG_PATH_DEFAULT   = "config/default.json";
static const char* DEFAULT_LEVEL_PATH    = "levels/default/level.json";
static const char* SHADER_VERT_PATH     = "shaders/vert.spv";
static const char* SHADER_FRAG_PATH     = "shaders/frag.spv";
static const char* SHADER_FRAG_UNTEX_PATH = "shaders/frag_untextured.spv";
static const char* SHADER_FRAG_ALT_PATH = "shaders/frag_alt.spv";
static const char* PIPELINE_KEY_MAIN_TEX   = "main_tex";
static const char* PIPELINE_KEY_WIRE_TEX   = "wire_tex";
static const char* PIPELINE_KEY_MASK_TEX   = "mask_tex";
static const char* PIPELINE_KEY_TRANSPARENT_TEX = "transparent_tex";
static const char* PIPELINE_KEY_MAIN_UNTEX = "main_untex";
static const char* PIPELINE_KEY_WIRE_UNTEX = "wire_untex";
static const char* PIPELINE_KEY_MASK_UNTEX = "mask_untex";
static const char* PIPELINE_KEY_TRANSPARENT_UNTEX = "transparent_untex";
static const char* PIPELINE_KEY_ALT     = "alt";
static const char* LAYOUT_KEY_MAIN_FRAG_TEX = "main_frag_tex";
static constexpr float kDefaultPanSpeed = 0.012f;
static constexpr float kOrthoFallbackHalfExtent = 8.f;

namespace {
    /** Map solid pipeline key to wireframe equivalent. Returns original if no wireframe variant exists. */
    std::string GetWireframePipelineKey(const std::string& solidKey) {
        if (solidKey == "main_tex" || solidKey == "transparent_tex") return "wire_tex";
        if (solidKey == "main_untex" || solidKey == "transparent_untex") return "wire_untex";
        if (solidKey == "mask_tex") return "wire_tex";
        if (solidKey == "mask_untex") return "wire_untex";
        // Already a wire pipeline or unknown
        return solidKey;
    }
    
    uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }
        throw std::runtime_error("FindMemoryType: no suitable memory type");
    }
}

VulkanApp::VulkanApp(const VulkanConfig& config_in)
    : m_config(config_in)
    , m_completedJobHandler([this](LoadJobType eType_ic, const std::string& sPath_ic, std::vector<uint8_t> vecData_in) {
          this->OnCompletedLoadJob(eType_ic, sPath_ic, std::move(vecData_in));
      }) {
    VulkanUtils::LogTrace("VulkanApp constructor");
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
    if constexpr (VulkanUtils::ENABLE_VALIDATION_LAYERS) {
        vecExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

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
    std::string sFragUntexPath = VulkanUtils::GetResourcePath(SHADER_FRAG_UNTEX_PATH);
    std::string sFragAltPath = VulkanUtils::GetResourcePath(SHADER_FRAG_ALT_PATH);
    
    // Warn about outdated shaders - only frag.frag (textured PBR) is fully up-to-date
    VulkanUtils::LogWarn("Shader frag_untextured.spv is OUTDATED: uses old GeometrySmith instead of V_GGX; prefer textured pipeline with default textures");
    VulkanUtils::LogWarn("Shader frag_alt.spv is OUTDATED: debug grayscale shader only, not PBR compliant");
    
    this->m_pipelineManager.RequestPipeline(PIPELINE_KEY_MAIN_TEX, &this->m_shaderManager, sVertPath, sFragPath);
    this->m_pipelineManager.RequestPipeline(PIPELINE_KEY_WIRE_TEX, &this->m_shaderManager, sVertPath, sFragPath);
    this->m_pipelineManager.RequestPipeline(PIPELINE_KEY_MASK_TEX, &this->m_shaderManager, sVertPath, sFragPath);
    this->m_pipelineManager.RequestPipeline(PIPELINE_KEY_TRANSPARENT_TEX, &this->m_shaderManager, sVertPath, sFragPath);
    this->m_pipelineManager.RequestPipeline(PIPELINE_KEY_MAIN_UNTEX, &this->m_shaderManager, sVertPath, sFragUntexPath);
    this->m_pipelineManager.RequestPipeline(PIPELINE_KEY_WIRE_UNTEX, &this->m_shaderManager, sVertPath, sFragUntexPath);
    this->m_pipelineManager.RequestPipeline(PIPELINE_KEY_MASK_UNTEX, &this->m_shaderManager, sVertPath, sFragUntexPath);
    this->m_pipelineManager.RequestPipeline(PIPELINE_KEY_TRANSPARENT_UNTEX, &this->m_shaderManager, sVertPath, sFragUntexPath);
    this->m_pipelineManager.RequestPipeline(PIPELINE_KEY_ALT, &this->m_shaderManager, sVertPath, sFragAltPath);

    /* Descriptor set layouts by key (before materials so pipeline layouts can reference them). */
    static const std::string kLayoutKeyMainFragTex("main_frag_tex");
    this->m_descriptorSetLayoutManager.SetDevice(this->m_device.GetDevice());
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            {
                .binding            = 0u,
                .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount    = 1u,
                .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr,
            },
            {
                .binding            = 1u,
                .descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount    = 1u,
                .stageFlags         = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
                .pImmutableSamplers = nullptr,
            },
            {
                .binding            = 2u,
                .descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount    = 1u,
                .stageFlags         = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
                .pImmutableSamplers = nullptr,
            },
            {
                .binding            = 3u,
                .descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount    = 1u,
                .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr,
            },
            {
                .binding            = 4u,
                .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount    = 1u,
                .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr,
            },
            {
                .binding            = 5u,
                .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount    = 1u,
                .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr,
            },
            {
                .binding            = 6u,
                .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount    = 1u,
                .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr,
            },
            {
                .binding            = 7u,
                .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount    = 1u,
                .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr,
            }
        };
        if (this->m_descriptorSetLayoutManager.RegisterLayout(kLayoutKeyMainFragTex, bindings) == VK_NULL_HANDLE)
            throw std::runtime_error("VulkanApp::InitVulkan: descriptor set layout main_frag_tex failed");
    }

    // Use instanced push constants (96 bytes) for batched instanced rendering
    constexpr uint32_t kMainPushConstantSize = kInstancedPushConstantSize;
    VkDescriptorSetLayout pMainFragLayout = this->m_descriptorSetLayoutManager.GetLayout(kLayoutKeyMainFragTex);
    PipelineLayoutDescriptor stTexturedLayoutDesc = {
        .pushConstantRanges = {
            { .stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), .offset = 0u, .size = kMainPushConstantSize }
        },
        .descriptorSetLayouts = { pMainFragLayout },
    };
    PipelineLayoutDescriptor stUntexturedLayoutDesc = {
        .pushConstantRanges = {
            { .stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), .offset = 0u, .size = kMainPushConstantSize }
        },
        .descriptorSetLayouts = { pMainFragLayout },
    };
    // glTF 2.0 spec mandates counter-clockwise winding for front faces.
    // We use CCW here to match the spec. DoubleSided materials disable culling entirely.
    GraphicsPipelineParams stPipeParamsMain = {
        .topology                = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable  = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = static_cast<VkCullModeFlags>((this->m_config.bCullBackFaces == true) ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE),
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth               = static_cast<float>(1.0f),
        .rasterizationSamples    = VK_SAMPLE_COUNT_1_BIT,
    };
    // Double-sided variant: always disable culling regardless of config
    GraphicsPipelineParams stPipeParamsDoubleSided = stPipeParamsMain;
    stPipeParamsDoubleSided.cullMode = VK_CULL_MODE_NONE;
    GraphicsPipelineParams stPipeParamsWire = stPipeParamsMain;
    stPipeParamsWire.polygonMode = VK_POLYGON_MODE_LINE;
    GraphicsPipelineParams stPipeParamsMask = stPipeParamsMain;
    GraphicsPipelineParams stPipeParamsTransparent = stPipeParamsMain;
    stPipeParamsTransparent.blendEnable = VK_TRUE;
    stPipeParamsTransparent.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    stPipeParamsTransparent.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    stPipeParamsTransparent.colorBlendOp = VK_BLEND_OP_ADD;
    stPipeParamsTransparent.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    stPipeParamsTransparent.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    stPipeParamsTransparent.alphaBlendOp = VK_BLEND_OP_ADD;
    stPipeParamsTransparent.depthWriteEnable = VK_FALSE;

    // Single-sided materials (use configured culling)
    this->m_cachedMaterials.push_back(this->m_materialManager.RegisterMaterial("main_tex", PIPELINE_KEY_MAIN_TEX, stTexturedLayoutDesc, stPipeParamsMain));
    this->m_cachedMaterials.push_back(this->m_materialManager.RegisterMaterial("wire_tex", PIPELINE_KEY_WIRE_TEX, stTexturedLayoutDesc, stPipeParamsWire));
    this->m_cachedMaterials.push_back(this->m_materialManager.RegisterMaterial("mask_tex", PIPELINE_KEY_MASK_TEX, stTexturedLayoutDesc, stPipeParamsMask));
    this->m_cachedMaterials.push_back(this->m_materialManager.RegisterMaterial("transparent_tex", PIPELINE_KEY_TRANSPARENT_TEX, stTexturedLayoutDesc, stPipeParamsTransparent));
    this->m_cachedMaterials.push_back(this->m_materialManager.RegisterMaterial("main_untex", PIPELINE_KEY_MAIN_UNTEX, stUntexturedLayoutDesc, stPipeParamsMain));
    this->m_cachedMaterials.push_back(this->m_materialManager.RegisterMaterial("wire_untex", PIPELINE_KEY_WIRE_UNTEX, stUntexturedLayoutDesc, stPipeParamsWire));
    this->m_cachedMaterials.push_back(this->m_materialManager.RegisterMaterial("mask_untex", PIPELINE_KEY_MASK_UNTEX, stUntexturedLayoutDesc, stPipeParamsMask));
    this->m_cachedMaterials.push_back(this->m_materialManager.RegisterMaterial("transparent_untex", PIPELINE_KEY_TRANSPARENT_UNTEX, stUntexturedLayoutDesc, stPipeParamsTransparent));
    this->m_cachedMaterials.push_back(this->m_materialManager.RegisterMaterial("alt",  PIPELINE_KEY_ALT,  stUntexturedLayoutDesc, stPipeParamsMain));
    // Double-sided material variants (glTF doubleSided=true)
    this->m_cachedMaterials.push_back(this->m_materialManager.RegisterMaterial("main_tex_ds", PIPELINE_KEY_MAIN_TEX, stTexturedLayoutDesc, stPipeParamsDoubleSided));
    this->m_cachedMaterials.push_back(this->m_materialManager.RegisterMaterial("mask_tex_ds", PIPELINE_KEY_MASK_TEX, stTexturedLayoutDesc, stPipeParamsDoubleSided));
    this->m_cachedMaterials.push_back(this->m_materialManager.RegisterMaterial("transparent_tex_ds", PIPELINE_KEY_TRANSPARENT_TEX, stTexturedLayoutDesc, stPipeParamsDoubleSided));
    this->m_cachedMaterials.push_back(this->m_materialManager.RegisterMaterial("main_untex_ds", PIPELINE_KEY_MAIN_UNTEX, stUntexturedLayoutDesc, stPipeParamsDoubleSided));
    this->m_cachedMaterials.push_back(this->m_materialManager.RegisterMaterial("mask_untex_ds", PIPELINE_KEY_MASK_UNTEX, stUntexturedLayoutDesc, stPipeParamsDoubleSided));
    this->m_cachedMaterials.push_back(this->m_materialManager.RegisterMaterial("transparent_untex_ds", PIPELINE_KEY_TRANSPARENT_UNTEX, stUntexturedLayoutDesc, stPipeParamsDoubleSided));
    this->m_meshManager.SetDevice(this->m_device.GetDevice());
    this->m_meshManager.SetPhysicalDevice(this->m_device.GetPhysicalDevice());
    this->m_meshManager.SetQueue(this->m_device.GetGraphicsQueue());
    this->m_meshManager.SetQueueFamilyIndex(this->m_device.GetQueueFamilyIndices().graphicsFamily);
    this->m_textureManager.SetDevice(this->m_device.GetDevice());
    this->m_textureManager.SetPhysicalDevice(this->m_device.GetPhysicalDevice());
    this->m_textureManager.SetQueue(this->m_device.GetGraphicsQueue());
    this->m_textureManager.SetQueueFamilyIndex(this->m_device.GetQueueFamilyIndices().graphicsFamily);
    this->m_sceneManager.SetDependencies(&this->m_materialManager, &this->m_meshManager, &this->m_textureManager);
    this->m_meshManager.SetJobQueue(&this->m_jobQueue);
    this->m_textureManager.SetJobQueue(&this->m_jobQueue);
    
    /* Start resource manager thread for async cleanup */
    this->m_resourceManagerThread.Start();
    
    /* Register all managers with cleanup orchestrator */
    this->m_resourceCleanupManager.SetManagers(
        &this->m_materialManager,
        &this->m_meshManager,
        &this->m_textureManager,
        &this->m_pipelineManager,
        &this->m_shaderManager
    );
    
    // Load level from config (set via command-line)
    if (m_config.sLevelPath.empty()) {
        VulkanUtils::LogErr("No level path specified in config");
        throw std::runtime_error("Level path required");
    }
    std::string levelPath = VulkanUtils::GetResourcePath(m_config.sLevelPath);
    if (!this->m_sceneManager.LoadLevelFromFile(levelPath)) {
        VulkanUtils::LogErr("Failed to load level: {}", levelPath);
        this->m_sceneManager.SetCurrentScene(std::make_unique<Scene>("empty"));
    }
    
    /* Set up scene change callback to invalidate batched draw list.
       This ensures batches are rebuilt only when scene structure changes, not every frame. */
    Scene* pLoadedScene = this->m_sceneManager.GetCurrentScene();
    if (pLoadedScene) {
        pLoadedScene->SetChangeCallback([this]() {
            this->m_batchedDrawList.SetDirty();
        });
    }

    /* Descriptor pool (sized from layout keys) and one set for "main" pipeline. */
    this->m_descriptorPoolManager.SetDevice(this->m_device.GetDevice());
    this->m_descriptorPoolManager.SetLayoutManager(&this->m_descriptorSetLayoutManager);
    // Set device limit for descriptor sets (use maxDescriptorSetSamplers as practical limit)
    this->m_descriptorPoolManager.SetDeviceLimit(this->m_device.GetMaxDescriptorSets());
    // Start with reasonable initial capacity (256), will grow dynamically up to device limit
    std::vector<std::string> poolLayouts = { kLayoutKeyMainFragTex };
    if (!this->m_descriptorPoolManager.BuildPool(poolLayouts, 256u))
        throw std::runtime_error("VulkanApp::InitVulkan: descriptor pool failed");
    this->m_descriptorSetMain = this->m_descriptorPoolManager.AllocateSet(kLayoutKeyMainFragTex);
    if (this->m_descriptorSetMain == VK_NULL_HANDLE)
        throw std::runtime_error("VulkanApp::InitVulkan: descriptor set allocation failed");
    
    /* Create object data SSBO (Storage Buffer for Per-Object Data).
       4096 objects × 256 bytes each = 1MB total. Updated each frame with all object data.
       GPU accesses via dynamic offsets: offset = objectIndex × 256. */
    {
        constexpr uint32_t MAX_OBJECTS = 4096;
        constexpr uint32_t OBJECT_DATA_SIZE = 256;  // sizeof(ObjectData)
        constexpr VkDeviceSize bufSize = MAX_OBJECTS * OBJECT_DATA_SIZE;  // 1MB
        
        VkBufferCreateInfo bufInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = static_cast<VkBufferCreateFlags>(0),
            .size = bufSize,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
        };
        
        VkResult r = vkCreateBuffer(this->m_device.GetDevice(), &bufInfo, nullptr, &this->m_objectDataBuffer);
        if (r != VK_SUCCESS) {
            VulkanUtils::LogErr("vkCreateBuffer (object data SSBO) failed: {}", static_cast<int>(r));
            throw std::runtime_error("VulkanApp::InitVulkan: object data buffer creation failed");
        }
        
        VkMemoryRequirements memReqs{};
        vkGetBufferMemoryRequirements(this->m_device.GetDevice(), this->m_objectDataBuffer, &memReqs);
        
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = FindMemoryType(this->m_device.GetPhysicalDevice(), memReqs.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        };
        
        r = vkAllocateMemory(this->m_device.GetDevice(), &allocInfo, nullptr, &this->m_objectDataMemory);
        if (r != VK_SUCCESS) {
            vkDestroyBuffer(this->m_device.GetDevice(), this->m_objectDataBuffer, nullptr);
            this->m_objectDataBuffer = VK_NULL_HANDLE;
            VulkanUtils::LogErr("vkAllocateMemory (object data) failed: {}", static_cast<int>(r));
            throw std::runtime_error("VulkanApp::InitVulkan: object data memory allocation failed");
        }
        
        vkBindBufferMemory(this->m_device.GetDevice(), this->m_objectDataBuffer, this->m_objectDataMemory, 0);
        VulkanUtils::LogInfo("Object data SSBO created: {} objects × {} bytes = {} MB", MAX_OBJECTS, OBJECT_DATA_SIZE, (bufSize / 1024 / 1024));
    }
    
    /* Create LightManager which owns the light SSBO.
       16 bytes header (light count) + 256 lights × 64 bytes = ~16KB.
       Updated each frame from SceneNew lights. */
    this->m_lightManager.Create(this->m_device.GetDevice(), this->m_device.GetPhysicalDevice());
    
    // Also keep the raw buffer handles for legacy code paths (will be removed after full migration)
    this->m_lightBuffer = this->m_lightManager.GetLightBuffer();
    // Note: m_lightBufferMemory is now managed by LightManager
    
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

    /* Initialize light debug renderer if enabled. Creates separate pipeline for debug line drawing. */
    if (this->m_config.bShowLightDebug) {
        if (!this->m_lightDebugRenderer.Create(this->m_device.GetDevice(), this->m_renderPass.Get(), this->m_device.GetPhysicalDevice())) {
            VulkanUtils::LogErr("Failed to create light debug renderer (continuing without debug visualization)");
        }
    }

#if EDITOR_BUILD
    /* Initialize editor layer (ImGui + ImGuizmo). */
    this->m_editorLayer.Init(
        this->m_pWindow->GetSDLWindow(),
        this->m_instance.Get(),
        this->m_device.GetPhysicalDevice(),
        this->m_device.GetDevice(),
        this->m_device.GetQueueFamilyIndices().graphicsFamily,
        this->m_device.GetGraphicsQueue(),
        this->m_renderPass.Get(),
        this->m_swapchain.GetImageCount()
    );
    /* Set level path for editor save functionality. */
    this->m_editorLayer.SetLevelPath(VulkanUtils::GetResourcePath(this->m_config.sLevelPath));
#else
    /* Initialize runtime overlay (minimal stats display). */
    this->m_runtimeOverlay.Init(
        this->m_pWindow->GetSDLWindow(),
        this->m_instance.Get(),
        this->m_device.GetPhysicalDevice(),
        this->m_device.GetDevice(),
        this->m_device.GetQueueFamilyIndices().graphicsFamily,
        this->m_device.GetGraphicsQueue(),
        this->m_renderPass.Get(),
        this->m_swapchain.GetImageCount()
    );
#endif

    /* Initialize multi-viewport manager. */
    VkExtent2D swapExtent = this->m_swapchain.GetExtent();
    // Get formats matching the main render pass for viewport render pass compatibility
    VkFormat viewportColorFormat = this->m_swapchain.GetImageFormat();
    const VkFormat pDepthCandidatesVp[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
    VkFormat viewportDepthFormat = VulkanDepthImage::FindSupportedFormat(
        this->m_device.GetPhysicalDevice(), pDepthCandidatesVp, 
        static_cast<uint32_t>(sizeof(pDepthCandidatesVp) / sizeof(pDepthCandidatesVp[0])));
    
    this->m_viewportManager.Create(
        this->m_device.GetDevice(),
        this->m_device.GetPhysicalDevice(),
        this->m_renderPass.Get(),
        VK_NULL_HANDLE,  // ImGui descriptor pool not needed for now
        viewportColorFormat,
        viewportDepthFormat,
        swapExtent.width,
        swapExtent.height
    );

}

void VulkanApp::EnsureMainDescriptorSetWritten() {
    if (this->m_descriptorSetMain == VK_NULL_HANDLE)
        return;
    /* Already exposed main/wire in the map → set was written. */
    auto it = this->m_pipelineDescriptorSets.find(PIPELINE_KEY_MAIN_TEX);
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
    
    VkDescriptorBufferInfo stBufferInfo = {
        .buffer = this->m_objectDataBuffer,
        .offset = 0,
        .range  = VK_WHOLE_SIZE,  /* Entire SSBO buffer available for dynamic offset access */
    };
    
    VkDescriptorBufferInfo stLightBufferInfo = {
        .buffer = this->m_lightBuffer,
        .offset = 0,
        .range  = VK_WHOLE_SIZE,
    };
    
    /* Default MR texture: white (1,1,1,1) so metallic/roughness factors are used as-is */
    VkDescriptorImageInfo stMRImageInfo = {
        .sampler     = pDefaultTex->GetSampler(),
        .imageView   = pDefaultTex->GetView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    
    /* Default emissive texture: white (1,1,1,1) so emissiveFactor is used as-is */
    VkDescriptorImageInfo stEmissiveImageInfo = {
        .sampler     = pDefaultTex->GetSampler(),
        .imageView   = pDefaultTex->GetView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    
    std::array<VkWriteDescriptorSet, 5> writeDescriptors = {{
        {
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
        },
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = this->m_descriptorSetMain,
            .dstBinding       = 2,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo       = nullptr,
            .pBufferInfo      = &stBufferInfo,
            .pTexelBufferView = nullptr,
        },
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = this->m_descriptorSetMain,
            .dstBinding       = 3,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo       = nullptr,
            .pBufferInfo      = &stLightBufferInfo,
            .pTexelBufferView = nullptr,
        },
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = this->m_descriptorSetMain,
            .dstBinding       = 4,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = &stMRImageInfo,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        },
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = this->m_descriptorSetMain,
            .dstBinding       = 5,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = &stEmissiveImageInfo,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        }
    }};
    
    vkUpdateDescriptorSets(this->m_device.GetDevice(), static_cast<uint32_t>(writeDescriptors.size()), writeDescriptors.data(), 0, nullptr);
    
    /* Register descriptor set for all pipeline keys (both textured and untextured). */
    this->m_pipelineDescriptorSets[std::string(PIPELINE_KEY_MAIN_TEX)] = { this->m_descriptorSetMain };
    this->m_pipelineDescriptorSets[std::string(PIPELINE_KEY_WIRE_TEX)] = { this->m_descriptorSetMain };
    this->m_pipelineDescriptorSets[std::string(PIPELINE_KEY_MASK_TEX)] = { this->m_descriptorSetMain };
    this->m_pipelineDescriptorSets[std::string(PIPELINE_KEY_TRANSPARENT_TEX)] = { this->m_descriptorSetMain };
    this->m_pipelineDescriptorSets[std::string(PIPELINE_KEY_MAIN_UNTEX)] = { this->m_descriptorSetMain };
    this->m_pipelineDescriptorSets[std::string(PIPELINE_KEY_WIRE_UNTEX)] = { this->m_descriptorSetMain };
    this->m_pipelineDescriptorSets[std::string(PIPELINE_KEY_MASK_UNTEX)] = { this->m_descriptorSetMain };
    this->m_pipelineDescriptorSets[std::string(PIPELINE_KEY_TRANSPARENT_UNTEX)] = { this->m_descriptorSetMain };
    this->m_pipelineDescriptorSets[std::string(PIPELINE_KEY_ALT)] = { this->m_descriptorSetMain };
}

VkDescriptorSet VulkanApp::GetOrCreateDescriptorSetForTexture(std::shared_ptr<TextureHandle> pTexture) {
    if (!pTexture || !pTexture->IsValid())
        return VK_NULL_HANDLE;
    
    TextureHandle* pRawTexture = pTexture.get();
    
    // Check cache
    auto it = m_textureDescriptorSets.find(pRawTexture);
    if (it != m_textureDescriptorSets.end())
        return it->second;
    
    // Allocate new descriptor set
    VkDescriptorSet newSet = m_descriptorPoolManager.AllocateSet(std::string("main_frag_tex")); // Same layout as main descriptor set
    if (newSet == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("GetOrCreateDescriptorSetForTexture: failed to allocate descriptor set");
        return VK_NULL_HANDLE;
    }
    
    // Get default MR texture (white = metallic/roughness factors used as-is)
    std::shared_ptr<TextureHandle> pDefaultMRTex = m_textureManager.GetOrCreateDefaultTexture();
    if (!pDefaultMRTex || !pDefaultMRTex->IsValid()) {
        VulkanUtils::LogErr("GetOrCreateDescriptorSetForTexture: failed to get default MR texture");
        return VK_NULL_HANDLE;
    }
    
    // Write texture to descriptor set
    VkDescriptorImageInfo imageInfo = {
        .sampler     = pTexture->GetSampler(),
        .imageView   = pTexture->GetView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    
    VkDescriptorBufferInfo bufferInfo = {
        .buffer = this->m_objectDataBuffer,
        .offset = 0,
        .range  = VK_WHOLE_SIZE,
    };
    
    VkDescriptorBufferInfo lightBufferInfo = {
        .buffer = this->m_lightBuffer,
        .offset = 0,
        .range  = VK_WHOLE_SIZE,
    };
    
    /* Default MR texture: white (1,1,1,1) so metallic/roughness factors are used as-is */
    VkDescriptorImageInfo mrImageInfo = {
        .sampler     = pDefaultMRTex->GetSampler(),
        .imageView   = pDefaultMRTex->GetView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    
    std::array<VkWriteDescriptorSet, 4> writes = {{
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = newSet,
            .dstBinding       = 0,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = &imageInfo,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        },
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = newSet,
            .dstBinding       = 2,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo       = nullptr,
            .pBufferInfo      = &bufferInfo,
            .pTexelBufferView = nullptr,
        },
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = newSet,
            .dstBinding       = 3,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo       = nullptr,
            .pBufferInfo      = &lightBufferInfo,
            .pTexelBufferView = nullptr,
        },
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = newSet,
            .dstBinding       = 4,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = &mrImageInfo,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        }
    }};
    vkUpdateDescriptorSets(m_device.GetDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    
    // Cache it (with reference to keep texture alive)
    m_textureDescriptorSets[pRawTexture] = newSet;
    m_descriptorSetTextures[newSet] = pTexture;
    
    return newSet;
}

VkDescriptorSet VulkanApp::GetOrCreateDescriptorSetForTextures(std::shared_ptr<TextureHandle> pBaseColorTexture,
                                                                std::shared_ptr<TextureHandle> pMetallicRoughnessTexture,
                                                                std::shared_ptr<TextureHandle> pEmissiveTexture,
                                                                std::shared_ptr<TextureHandle> pNormalTexture,
                                                                std::shared_ptr<TextureHandle> pOcclusionTexture) {
    if (!pBaseColorTexture || !pBaseColorTexture->IsValid())
        return VK_NULL_HANDLE;
    
    // Create combined cache key from all texture pointers
    TextureHandle* pRawBaseColor = pBaseColorTexture.get();
    TextureHandle* pRawMR = pMetallicRoughnessTexture ? pMetallicRoughnessTexture.get() : nullptr;
    TextureHandle* pRawEmissive = pEmissiveTexture ? pEmissiveTexture.get() : nullptr;
    TextureHandle* pRawNormal = pNormalTexture ? pNormalTexture.get() : nullptr;
    TextureHandle* pRawOcclusion = pOcclusionTexture ? pOcclusionTexture.get() : nullptr;
    
    // Use tuple cache key for (baseColor, MR, emissive, normal, occlusion)
    auto cacheKey = std::make_tuple(pRawBaseColor, pRawMR, pRawEmissive, pRawNormal, pRawOcclusion);
    
    auto it = m_textureQuintupleDescriptorSets.find(cacheKey);
    if (it != m_textureQuintupleDescriptorSets.end())
        return it->second;
    
    // Allocate new descriptor set
    VkDescriptorSet newSet = m_descriptorPoolManager.AllocateSet(std::string("main_frag_tex"));
    if (newSet == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("GetOrCreateDescriptorSetForTextures: failed to allocate descriptor set");
        return VK_NULL_HANDLE;
    }
    
    // Get default texture for MR and emissive (white = factors used as-is)
    std::shared_ptr<TextureHandle> pDefaultTex = m_textureManager.GetOrCreateDefaultTexture();
    if (!pDefaultTex || !pDefaultTex->IsValid()) {
        VulkanUtils::LogErr("GetOrCreateDescriptorSetForTextures: failed to get default texture");
        return VK_NULL_HANDLE;
    }
    
    std::shared_ptr<TextureHandle> pMRToUse = (pMetallicRoughnessTexture && pMetallicRoughnessTexture->IsValid()) 
        ? pMetallicRoughnessTexture : pDefaultTex;
    std::shared_ptr<TextureHandle> pEmissiveToUse = (pEmissiveTexture && pEmissiveTexture->IsValid()) 
        ? pEmissiveTexture : pDefaultTex;
    std::shared_ptr<TextureHandle> pNormalToUse = (pNormalTexture && pNormalTexture->IsValid()) 
        ? pNormalTexture : pDefaultTex;
    std::shared_ptr<TextureHandle> pOcclusionToUse = (pOcclusionTexture && pOcclusionTexture->IsValid()) 
        ? pOcclusionTexture : pDefaultTex;
    
    // Write textures to descriptor set
    VkDescriptorImageInfo baseColorImageInfo = {
        .sampler     = pBaseColorTexture->GetSampler(),
        .imageView   = pBaseColorTexture->GetView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    
    VkDescriptorBufferInfo bufferInfo = {
        .buffer = this->m_objectDataBuffer,
        .offset = 0,
        .range  = VK_WHOLE_SIZE,
    };
    
    VkDescriptorBufferInfo lightBufferInfo = {
        .buffer = this->m_lightBuffer,
        .offset = 0,
        .range  = VK_WHOLE_SIZE,
    };
    
    VkDescriptorImageInfo mrImageInfo = {
        .sampler     = pMRToUse->GetSampler(),
        .imageView   = pMRToUse->GetView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    
    VkDescriptorImageInfo emissiveImageInfo = {
        .sampler     = pEmissiveToUse->GetSampler(),
        .imageView   = pEmissiveToUse->GetView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    
    VkDescriptorImageInfo normalImageInfo = {
        .sampler     = pNormalToUse->GetSampler(),
        .imageView   = pNormalToUse->GetView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    
    VkDescriptorImageInfo occlusionImageInfo = {
        .sampler     = pOcclusionToUse->GetSampler(),
        .imageView   = pOcclusionToUse->GetView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    
    std::array<VkWriteDescriptorSet, 7> writes = {{
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = newSet,
            .dstBinding       = 0,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = &baseColorImageInfo,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        },
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = newSet,
            .dstBinding       = 2,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo       = nullptr,
            .pBufferInfo      = &bufferInfo,
            .pTexelBufferView = nullptr,
        },
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = newSet,
            .dstBinding       = 3,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo       = nullptr,
            .pBufferInfo      = &lightBufferInfo,
            .pTexelBufferView = nullptr,
        },
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = newSet,
            .dstBinding       = 4,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = &mrImageInfo,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        },
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = newSet,
            .dstBinding       = 5,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = &emissiveImageInfo,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        },
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = newSet,
            .dstBinding       = 6,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = &normalImageInfo,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        },
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = newSet,
            .dstBinding       = 7,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = &occlusionImageInfo,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        }
    }};
    vkUpdateDescriptorSets(m_device.GetDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    
    // Cache it
    m_textureQuintupleDescriptorSets[cacheKey] = newSet;
    
    return newSet;
}

void VulkanApp::CleanupUnusedTextureDescriptorSets() {
    if (m_sceneManager.GetCurrentScene() == nullptr)
        return;
    
    // Collect textures still in use by current scene
    std::set<TextureHandle*> texturesInUse;
    const Scene* pScene = m_sceneManager.GetCurrentScene();
    for (const Object& obj : pScene->GetObjects()) {
        if (obj.pTexture && obj.pTexture->IsValid()) {
            texturesInUse.insert(obj.pTexture.get());
        }
    }
    
    // Also keep default texture alive
    if (m_pDefaultTexture && m_pDefaultTexture->IsValid()) {
        texturesInUse.insert(m_pDefaultTexture.get());
    }
    
    // Find unused descriptor sets
    std::vector<VkDescriptorSet> setsToFree;
    for (auto it = m_textureDescriptorSets.begin(); it != m_textureDescriptorSets.end(); ) {
        if (texturesInUse.find(it->first) == texturesInUse.end()) {
            setsToFree.push_back(it->second);
            m_descriptorSetTextures.erase(it->second);
            it = m_textureDescriptorSets.erase(it);
        } else {
            ++it;
        }
    }
    
    // Free unused descriptor sets
    for (VkDescriptorSet set : setsToFree) {
        m_descriptorPoolManager.FreeSet(set);
    }
    
    if (!setsToFree.empty()) {
        VulkanUtils::LogDebug("Cleaned up {} unused texture descriptor sets", setsToFree.size());
    }
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
    
    /* Mark batched draw list dirty since pipelines were destroyed.
       This ensures batches are rebuilt with new pipeline handles. */
    this->m_batchedDrawList.SetDirty();
    
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
        // Clean up unused texture descriptor sets before trimming textures
        CleanupUnusedTextureDescriptorSets();
        
        /* Enqueue unified resource cleanup to worker thread (non-blocking) */
        this->m_resourceManagerThread.EnqueueCommand(
            ResourceManagerThread::Command(ResourceManagerThread::CommandType::TrimAll,
                [this]() { this->m_resourceCleanupManager.TrimAllCaches(); })
        );

#if EDITOR_BUILD
        // Process events with editor handler (ImGui gets first pass)
        bQuit = this->m_pWindow->PollEventsWithHandler([this](const SDL_Event& evt) -> bool {
            return this->m_editorLayer.ProcessEvent(&evt);
        });
        
        // Begin editor frame
        this->m_editorLayer.BeginFrame();
#else
        // Runtime mode: poll events with overlay handler
        bQuit = this->m_pWindow->PollEventsWithHandler([this](const SDL_Event& evt) -> bool {
            // Toggle overlay with F3 key
            if (evt.type == SDL_EVENT_KEY_DOWN && evt.key.key == SDLK_F3) {
                this->m_runtimeOverlay.ToggleVisible();
                return true;
            }
            return this->m_runtimeOverlay.ProcessEvent(&evt);
        });
#endif
        if (bQuit == true)
            break;

#if EDITOR_BUILD
        // Skip camera update if editor wants input
        const bool bEditorWantsInput = this->m_editorLayer.WantCaptureMouse() || this->m_editorLayer.WantCaptureKeyboard();
#else
        // Runtime overlay: check if ImGui wants input
        const bool bEditorWantsInput = this->m_runtimeOverlay.WantCaptureMouse() || this->m_runtimeOverlay.WantCaptureKeyboard();
#endif

        const float fMoveSpeed = (this->m_config.fPanSpeed > 0.f) ? this->m_config.fPanSpeed : kDefaultPanSpeed;
        if (!bEditorWantsInput) {
            CameraController_Update(this->m_camera, SDL_GetKeyboardState(nullptr), fMoveSpeed, this->m_avgFrameTimeSec);
        }
        
        // Mouse look (right-click to capture mouse, Escape to release)
        float mouseDeltaX = 0.f, mouseDeltaY = 0.f;
        this->m_pWindow->GetMouseDelta(mouseDeltaX, mouseDeltaY);
        if ((mouseDeltaX != 0.f || mouseDeltaY != 0.f) && !bEditorWantsInput) {
            CameraController_MouseLook(this->m_camera, mouseDeltaX, mouseDeltaY);
        }

        if (this->m_pWindow->GetWindowMinimized() == true) {
            VulkanUtils::LogTrace("Window minimized, skipping draw");
#if EDITOR_BUILD
            // EndFrame must match BeginFrame to keep ImGui state consistent
            this->m_editorLayer.EndFrame();
#endif
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
        
        /* Store projection matrix in camera for editor gizmos. */
        this->m_camera.SetProjectionMatrix(glm::mat4(
            fProjMat4[0], fProjMat4[1], fProjMat4[2], fProjMat4[3],
            fProjMat4[4], fProjMat4[5], fProjMat4[6], fProjMat4[7],
            fProjMat4[8], fProjMat4[9], fProjMat4[10], fProjMat4[11],
            fProjMat4[12], fProjMat4[13], fProjMat4[14], fProjMat4[15]
        ));
        
        alignas(16) float fViewMat4[16];
        this->m_camera.GetViewMatrix(fViewMat4);
        alignas(16) float fViewProj[16];
        ObjectMat4Multiply(fViewProj, fProjMat4, fViewMat4);

        /* Get camera position for PBR specular calculations. */
        float fCamPos[3];
        this->m_camera.GetPosition(fCamPos[0], fCamPos[1], fCamPos[2]);

        Scene* pScene = this->m_sceneManager.GetCurrentScene();
        if (pScene != nullptr) {
            /* Update all objects with delta time (frame-rate independent). */
            pScene->UpdateAllObjects(this->m_avgFrameTimeSec);
            
            const auto& objects = pScene->GetObjects();
            for (size_t i = 0; i < objects.size(); ++i) {
                Object& obj = const_cast<Object&>(objects[i]);
                ObjectFillPushData(obj, fViewProj, static_cast<uint32_t>(i), fCamPos);
            }
        }

        /* Update object data SSBO: write all objects' per-object data (model matrix, emissive, material properties).
           Each object occupies 256 bytes at offset = objectIndex × 256.
           GPU accesses via push constant objectIndex to index into the SSBO array. */
        {
            constexpr uint32_t OBJECT_DATA_SIZE = 256;
            constexpr uint32_t MAX_OBJECTS = 4096;
            
            void* pMapped = nullptr;
            vkMapMemory(this->m_device.GetDevice(), this->m_objectDataMemory, 0, VK_WHOLE_SIZE, 0, &pMapped);
            if (pMapped) {
                uint8_t* pBuffer = static_cast<uint8_t*>(pMapped);
                const auto& objects = pScene->GetObjects();
                
                /* Write each object's data at its reserved offset. */
                for (size_t i = 0; i < objects.size() && i < MAX_OBJECTS; ++i) {
                    const Object& obj = objects[i];
                    ObjectData* pObjData = reinterpret_cast<ObjectData*>(pBuffer + i * OBJECT_DATA_SIZE);
                    
                    /* Model matrix (for normal transform, world position). */
                    pObjData->model = glm::mat4(
                        obj.localTransform[0],  obj.localTransform[1],  obj.localTransform[2],  obj.localTransform[3],
                        obj.localTransform[4],  obj.localTransform[5],  obj.localTransform[6],  obj.localTransform[7],
                        obj.localTransform[8],  obj.localTransform[9],  obj.localTransform[10], obj.localTransform[11],
                        obj.localTransform[12], obj.localTransform[13], obj.localTransform[14], obj.localTransform[15]
                    );
                    
                    /* Emissive color + strength (from glTF). */
                    pObjData->emissive = glm::vec4(obj.emissive[0], obj.emissive[1], obj.emissive[2], obj.emissive[3]);
                    
                    /* Material properties: metallic, roughness, normalScale, occlusionStrength (from glTF). */
                    pObjData->matProps = glm::vec4(obj.metallicFactor, obj.roughnessFactor, obj.normalScale, obj.occlusionStrength);
                    
                    /* Base color (from glTF baseColorFactor). */
                    pObjData->baseColor = glm::vec4(obj.color[0], obj.color[1], obj.color[2], obj.color[3]);
                    
                    /* Reserved fields for future use (phase 3+ extensions: lighting, animation, physics, etc). */
                    pObjData->reserved0 = glm::vec4(0.f);
                    pObjData->reserved1 = glm::vec4(0.f);
                    pObjData->reserved2 = glm::vec4(0.f);
                    pObjData->reserved3 = glm::vec4(0.f);
                    pObjData->reserved4 = glm::vec4(0.f);
                    pObjData->reserved5 = glm::vec4(0.f);
                    pObjData->reserved6 = glm::vec4(0.f);
                    pObjData->reserved7 = glm::vec4(0.f);
                    pObjData->reserved8 = glm::vec4(0.f);
                }
                
                vkUnmapMemory(this->m_device.GetDevice(), this->m_objectDataMemory);
            }
        } // End of SSBO write block
        
        /* Sync SceneNew transforms to legacy Scene Objects for rendering.
           This ensures editor changes to mesh transforms are reflected in the render. */
        this->m_sceneManager.SyncTransformsToScene(); 
        
        /* Sync emissive objects to proper Light entities in SceneNew.
           Creates/updates/removes LightComponents for Objects with emitsLight=true.
           All lights (scene lights + emissive lights) are now handled uniformly.
           Must be called BEFORE UpdateLightBuffer() so emissive lights are included. */
        this->m_sceneManager.SyncEmissiveLights();

        /* Update light buffer from SceneNew.
           This uploads light data from the ECS scene to the GPU light SSBO.
           All lights (scene lights + emissive lights from objects) are uploaded uniformly. */
        {
            SceneNew* pSceneNew = this->m_sceneManager.GetSceneNew();
            if (pSceneNew) {
                // Update all transform matrices before reading positions
                pSceneNew->UpdateAllTransforms();
                
                // Set scene on light manager if not already set
                this->m_lightManager.SetScene(pSceneNew);
                
                // Upload light data to GPU
                this->m_lightManager.UpdateLightBuffer();
            }
        }

        /* Ensure main descriptor set is written (default texture) before drawing main/wire; idempotent. */
        EnsureMainDescriptorSetWritten();

        /* Build draw list from scene (frustum culling, push size validation, sort by pipeline/mesh). */
        // Pass callback to get descriptor sets for per-object PBR textures (base color, metallic-roughness, emissive, normal, occlusion)
        auto getTextureDescriptorSet = [this](std::shared_ptr<TextureHandle> pBaseColor, 
                                              std::shared_ptr<TextureHandle> pMR,
                                              std::shared_ptr<TextureHandle> pEmissive,
                                              std::shared_ptr<TextureHandle> pNormal,
                                              std::shared_ptr<TextureHandle> pOcclusion) -> VkDescriptorSet {
            return this->GetOrCreateDescriptorSetForTextures(pBaseColor, pMR, pEmissive, pNormal, pOcclusion);
        };
        
        /* Use BatchedDrawList for efficient instanced rendering with dirty tracking.
           Only rebuilds batches when scene changes, not every frame.
           Editor uses viewport's offscreen render pass; Runtime uses main swapchain render pass. */
#if EDITOR_BUILD
        VkRenderPass offscreenRenderPass = this->m_viewportManager.GetOffscreenRenderPass();
        VkRenderPass renderPassForBatching = (offscreenRenderPass != VK_NULL_HANDLE) 
            ? offscreenRenderPass 
            : this->m_renderPass.Get();
        bool batchRenderPassHasDepth = (offscreenRenderPass != VK_NULL_HANDLE) 
            ? true 
            : this->m_renderPass.HasDepthAttachment();
#else
        // Runtime: render directly to swapchain using main render pass
        VkRenderPass renderPassForBatching = this->m_renderPass.Get();
        bool batchRenderPassHasDepth = this->m_renderPass.HasDepthAttachment();
#endif
        this->m_batchedDrawList.RebuildIfDirty(pScene,
                                  this->m_device.GetDevice(), renderPassForBatching, batchRenderPassHasDepth,
                                  &this->m_pipelineManager, &this->m_materialManager, &this->m_shaderManager,
                                  &this->m_pipelineDescriptorSets, getTextureDescriptorSet);
        
        /* Update visibility (frustum culling) each frame - fast operation on existing batches */
        this->m_batchedDrawList.UpdateVisibility(fViewProj, pScene);
        
        /* Convert visible objects to DrawCall format.
           We iterate visible object indices (frustum-culled) and look up batch info. */
        this->m_drawCalls.clear();
        const auto& visibleIndices = this->m_batchedDrawList.GetVisibleObjectIndices();
        this->m_drawCalls.reserve(visibleIndices.size());
        
        for (uint32_t objIdx : visibleIndices) {
            const auto* batch = this->m_batchedDrawList.GetBatchForObject(objIdx);
            if (!batch) continue;
            
            DrawCall dc = {
                .pipeline           = batch->pipeline,
                .pipelineLayout     = batch->pipelineLayout,
                .vertexBuffer       = batch->vertexBuffer,
                .vertexBufferOffset = batch->vertexBufferOffset,
                .pPushConstants     = nullptr,  // Push constants built per-viewport
                .pushConstantSize   = kInstancedPushConstantSize,
                .vertexCount        = batch->vertexCount,
                .instanceCount      = 1,  // One instance per draw call
                .firstVertex        = batch->firstVertex,
                .firstInstance      = 0,
                .descriptorSets     = batch->descriptorSets,
                .instanceBuffer     = VK_NULL_HANDLE,
                .instanceBufferOffset = 0,
                .dynamicOffsets     = {},
                .pLocalTransform    = nullptr,
                .color              = {1.0f, 1.0f, 1.0f, 1.0f},
                .objectIndex        = objIdx,  // Actual SSBO index for this object
                .pipelineKey        = batch->pipelineKey,
            };
            this->m_drawCalls.push_back(dc);
        }

#if EDITOR_BUILD
        /* Draw editor panels and gizmos, then end ImGui frame. */
        SceneNew* pSceneNewForEditor = this->m_sceneManager.GetSceneNew();
        Scene* pLegacySceneForEditor = this->m_sceneManager.GetCurrentScene();
        this->m_editorLayer.DrawEditor(pSceneNewForEditor, &this->m_camera, this->m_config, &this->m_viewportManager, pLegacySceneForEditor);
        this->m_editorLayer.EndFrame();
#else
        /* Update and draw runtime overlay (FPS, frame time, etc.). */
        this->m_runtimeOverlay.Update(this->m_avgFrameTimeSec);
        this->m_runtimeOverlay.Draw(&this->m_camera, &this->m_config);
#endif

        /* Always present (empty draw list = clear only) so swapchain and frame advance stay valid. */
        if (!DrawFrame(this->m_drawCalls, fViewProj))
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
    case LoadJobType::LoadMesh:
        this->m_meshManager.OnCompletedMeshFile(sPath_ic, std::move(vecData_in));
        break;
    case LoadJobType::LoadTexture:
        this->m_textureManager.OnCompletedTexture(sPath_ic, std::move(vecData_in));
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

#if EDITOR_BUILD
    this->m_editorLayer.Shutdown();
#else
    this->m_runtimeOverlay.ShutdownImGui();
#endif

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
    
    // Free all texture descriptor sets
    for (auto& pair : m_textureDescriptorSets) {
        if (pair.second != VK_NULL_HANDLE && m_descriptorPoolManager.IsValid()) {
            m_descriptorPoolManager.FreeSet(pair.second);
        }
    }
    m_textureDescriptorSets.clear();
    m_descriptorSetTextures.clear();
    
    if (this->m_descriptorSetMain != VK_NULL_HANDLE && this->m_descriptorPoolManager.IsValid()) {
        this->m_descriptorPoolManager.FreeSet(this->m_descriptorSetMain);
        this->m_descriptorSetMain = VK_NULL_HANDLE;
    }
    
    /* Clean up object data SSBO. */
    if (this->m_objectDataBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(this->m_device.GetDevice(), this->m_objectDataBuffer, nullptr);
        this->m_objectDataBuffer = VK_NULL_HANDLE;
    }
    if (this->m_objectDataMemory != VK_NULL_HANDLE) {
        vkFreeMemory(this->m_device.GetDevice(), this->m_objectDataMemory, nullptr);
        this->m_objectDataMemory = VK_NULL_HANDLE;
    }
    
    /* Clean up light manager (owns the light SSBO). */
    this->m_lightManager.Destroy();
    this->m_lightBuffer = VK_NULL_HANDLE;  // Was just a reference to LightManager's buffer
    
    /* Clean up light debug renderer. */
    this->m_lightDebugRenderer.Destroy();
    
    /* Clean up viewport manager. */
    this->m_viewportManager.Destroy();
    
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

bool VulkanApp::DrawFrame(const std::vector<DrawCall>& vecDrawCalls_ic, const float* pViewProjMat16_ic) {
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

    /* Build post-scene callback for ImGui rendering only (inside swapchain render pass). */
    std::function<void(VkCommandBuffer)> postSceneCallback = nullptr;
#if EDITOR_BUILD
    postSceneCallback = [this](VkCommandBuffer cmd) {
        // Render ImGui draw data (displays viewport textures)
        this->m_editorLayer.RenderDrawData(cmd);
    };
#else
    postSceneCallback = [this](VkCommandBuffer cmd) {
        // Render runtime overlay draw data
        this->m_runtimeOverlay.RenderDrawData(cmd);
    };
#endif

#if EDITOR_BUILD
    // Pre-scene callback for viewport rendering (all viewports render to offscreen targets)
    // This includes scene objects AND light debug
    std::function<void(VkCommandBuffer)> preSceneCallback = nullptr;
    const bool bRenderLightDebug = this->m_config.bShowLightDebug && this->m_lightDebugRenderer.IsReady() && pViewProjMat16_ic != nullptr;
    SceneNew* pSceneNew = this->m_sceneManager.GetSceneNew();
    
    preSceneCallback = [this, &vecDrawCalls_ic, bRenderLightDebug, pSceneNew, pViewProjMat16_ic](VkCommandBuffer cmd) {
        // Per-viewport temporary push constant buffer (96 bytes for instanced rendering)
        alignas(16) uint8_t vpPushData[kInstancedPushConstantSize];
        
        auto& vps = this->m_viewportManager.GetViewports();
        for (auto& vp : vps) {
            if (!vp.config.bVisible) {
                continue;
            }
            if (!vp.renderTarget.IsValid()) {
                continue;
            }
            
            // Get the camera for this viewport (main camera or scene camera)
            Camera* pVpCamera = this->m_viewportManager.GetCameraForViewport(vp, pSceneNew, &this->m_camera);
            if (!pVpCamera) {
                pVpCamera = &this->m_camera;  // Fallback to main camera
            }
            
            // Get camera position for this viewport
            float vpCamPos[3];
            pVpCamera->GetPosition(vpCamPos[0], vpCamPos[1], vpCamPos[2]);
            
            // Get view matrix from the viewport's camera
            alignas(16) float vpViewMat[16];
            pVpCamera->GetViewMatrix(vpViewMat);
            
            // Compute per-viewport projection matrix using viewport's aspect ratio
            const float vpAspect = (vp.renderTarget.height > 0) 
                ? static_cast<float>(vp.renderTarget.width) / static_cast<float>(vp.renderTarget.height)
                : 1.0f;
            
            alignas(16) float vpProjMat[16];
            if (this->m_config.bUsePerspective) {
                ObjectSetPerspective(vpProjMat, this->m_config.fCameraFovYRad, vpAspect, 
                                     this->m_config.fCameraNearZ, this->m_config.fCameraFarZ);
            } else {
                const float fH = (this->m_config.fOrthoHalfExtent > 0.f) 
                    ? this->m_config.fOrthoHalfExtent : kOrthoFallbackHalfExtent;
                ObjectSetOrtho(vpProjMat, -fH * vpAspect, fH * vpAspect, -fH, fH,
                               this->m_config.fOrthoNear, this->m_config.fOrthoFar);
            }
            
            // Combine projection and view for this viewport
            alignas(16) float vpViewProj[16];
            ObjectMat4Multiply(vpViewProj, vpProjMat, vpViewMat);
            
            // Begin viewport render pass
            this->m_viewportManager.BeginViewportRender(vp.config.id, cmd);
            
            // Determine if we need to switch to wireframe pipeline for this viewport
            const bool bWireframeMode = (vp.config.renderMode == ViewportRenderMode::Wireframe);
            
            // Render scene draw calls to this viewport with recomputed MVP
            for (const auto& dc : vecDrawCalls_ic) {
                // Select the appropriate pipeline based on viewport render mode
                VkPipeline pipelineToUse = dc.pipeline;
                
                if (bWireframeMode && !dc.pipelineKey.empty()) {
                    // Get the wireframe variant of this pipeline
                    std::string wireKey = GetWireframePipelineKey(dc.pipelineKey);
                    if (wireKey != dc.pipelineKey) {
                        // Look up the wireframe material/pipeline
                        auto pWireMat = this->m_materialManager.GetMaterial(wireKey);
                        if (pWireMat) {
                            // Get the pipeline from the material
                            VkPipeline wirePipe = pWireMat->GetPipelineIfReady(
                                this->m_device.GetDevice(),
                                this->m_viewportManager.GetOffscreenRenderPass(),
                                &this->m_pipelineManager,
                                &this->m_shaderManager,
                                true  // renderPassHasDepth
                            );
                            if (wirePipe != VK_NULL_HANDLE) {
                                pipelineToUse = wirePipe;
                            }
                        }
                    }
                }
                
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineToUse);
                if (!dc.descriptorSets.empty()) {
                    if (!dc.dynamicOffsets.empty()) {
                        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, dc.pipelineLayout,
                            0, static_cast<uint32_t>(dc.descriptorSets.size()), dc.descriptorSets.data(),
                            static_cast<uint32_t>(dc.dynamicOffsets.size()), dc.dynamicOffsets.data());
                    } else {
                        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, dc.pipelineLayout,
                            0, static_cast<uint32_t>(dc.descriptorSets.size()), dc.descriptorSets.data(),
                            0, nullptr);
                    }
                }
                
                // Recompute push constants with viewport-specific viewProj (instanced layout)
                if (dc.pushConstantSize == kInstancedPushConstantSize) {
                    // Instanced layout: viewProj (64) + camPos (16) + batchStartIndex (4) + padding (12) = 96 bytes
                    // objectIndex holds batchStartIndex for this batch
                    std::memcpy(vpPushData, vpViewProj, 64);  // viewProj at offset 0
                    std::memcpy(vpPushData + 64, vpCamPos, 12);  // camPos xyz at offset 64
                    float camW = 1.0f;
                    std::memcpy(vpPushData + 76, &camW, 4);  // camPos.w at offset 76
                    std::memcpy(vpPushData + 80, &dc.objectIndex, 4);  // batchStartIndex at offset 80
                    std::memset(vpPushData + 84, 0, 12);  // Padding at offset 84
                    
                    vkCmdPushConstants(cmd, dc.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, kInstancedPushConstantSize, vpPushData);
                } else if (dc.pushConstantSize > 0 && dc.pPushConstants != nullptr) {
                    // Fallback: use original push constants (legacy path)
                    vkCmdPushConstants(cmd, dc.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, dc.pushConstantSize, dc.pPushConstants);
                }
                
                VkDeviceSize offset = dc.vertexBufferOffset;
                vkCmdBindVertexBuffers(cmd, 0, 1, &dc.vertexBuffer, &offset);
                vkCmdDraw(cmd, dc.vertexCount, dc.instanceCount, dc.firstVertex, dc.firstInstance);
            }
            
            // Render light debug visualizations (inside the viewport render pass)
            if (bRenderLightDebug && pSceneNew) {
                this->m_lightDebugRenderer.Draw(cmd, pSceneNew, vpViewProj);
            }
            
            // End viewport render pass
            this->m_viewportManager.EndViewportRender(vp.config.id, cmd);
        }
    };
    
    // Editor mode: Scene renders to offscreen viewports via preSceneCallback
    // Main render pass only renders ImGui which displays the viewport textures
    std::vector<DrawCall> emptyDrawCalls;

    this->m_commandBuffers.Record(lImageIndex, this->m_renderPass.Get(),
                            this->m_framebuffers.Get()[lImageIndex],
                            stRenderArea, stViewport, stScissor, emptyDrawCalls,
                            vecClearValues.data(), lClearValueCount, preSceneCallback, postSceneCallback);
#else
    // Release/Runtime mode: Render scene directly to swapchain render pass
    // No viewport system - render directly to screen
    
    // Get camera matrices for main camera
    alignas(16) float rtViewMat[16];
    this->m_camera.GetViewMatrix(rtViewMat);
    
    float rtCamPos[3];
    this->m_camera.GetPosition(rtCamPos[0], rtCamPos[1], rtCamPos[2]);
    
    // Compute projection matrix for swapchain aspect ratio
    const float rtAspect = (stExtent.height > 0) 
        ? static_cast<float>(stExtent.width) / static_cast<float>(stExtent.height)
        : 1.0f;
    
    alignas(16) float rtProjMat[16];
    if (this->m_config.bUsePerspective) {
        ObjectSetPerspective(rtProjMat, this->m_config.fCameraFovYRad, rtAspect, 
                             this->m_config.fCameraNearZ, this->m_config.fCameraFarZ);
    } else {
        const float fH = (this->m_config.fOrthoHalfExtent > 0.f) 
            ? this->m_config.fOrthoHalfExtent : kOrthoFallbackHalfExtent;
        ObjectSetOrtho(rtProjMat, -fH * rtAspect, fH * rtAspect, -fH, fH,
                       this->m_config.fOrthoNear, this->m_config.fOrthoFar);
    }
    
    // Combine projection and view for Runtime rendering
    alignas(16) float rtViewProj[16];
    ObjectMat4Multiply(rtViewProj, rtProjMat, rtViewMat);
    
    // Resize push constant buffer to fit all draw calls
    this->m_runtimePushConstantBuffer.resize(vecDrawCalls_ic.size());
    
    // Build push constant data for each draw call using main camera's viewProj
    // Mutable copy of draw calls so we can set pPushConstants
    std::vector<DrawCall> runtimeDrawCalls = vecDrawCalls_ic;
    for (size_t i = 0; i < runtimeDrawCalls.size(); ++i) {
        auto& dc = runtimeDrawCalls[i];
        auto& pcData = this->m_runtimePushConstantBuffer[i];
        
        // Instanced layout: viewProj (64) + camPos (16) + batchStartIndex (4) + padding (12) = 96 bytes
        std::memcpy(pcData.data(), rtViewProj, 64);  // viewProj at offset 0
        std::memcpy(pcData.data() + 64, rtCamPos, 12);  // camPos xyz at offset 64
        float camW = 1.0f;
        std::memcpy(pcData.data() + 76, &camW, 4);  // camPos.w at offset 76
        std::memcpy(pcData.data() + 80, &dc.objectIndex, 4);  // batchStartIndex at offset 80
        std::memset(pcData.data() + 84, 0, 12);  // Padding at offset 84
        
        dc.pPushConstants = pcData.data();
        dc.pushConstantSize = kInstancedPushConstantSize;
    }
    
    // Pre-scene callback: light debug rendering (runtime doesn't have viewports)
    SceneNew* pSceneNew = this->m_sceneManager.GetSceneNew();
    const bool bRenderLightDebug = this->m_config.bShowLightDebug && this->m_lightDebugRenderer.IsReady() && pViewProjMat16_ic != nullptr;
    
    std::function<void(VkCommandBuffer)> preSceneCallback = nullptr;
    // No pre-scene callback for Runtime - we render directly in main pass
    
    // Post-scene callback for light debug and runtime overlay (reassign existing variable)
    postSceneCallback = [this, bRenderLightDebug, pSceneNew, &rtViewProj](VkCommandBuffer cmd) {
        // Render light debug (inside main render pass, after scene objects)
        if (bRenderLightDebug && pSceneNew) {
            this->m_lightDebugRenderer.Draw(cmd, pSceneNew, rtViewProj);
        }
        // Render runtime overlay draw data (FPS, etc.)
        this->m_runtimeOverlay.RenderDrawData(cmd);
    };
    
    // Runtime: Pass actual draw calls to render scene directly to swapchain
    this->m_commandBuffers.Record(lImageIndex, this->m_renderPass.Get(),
                            this->m_framebuffers.Get()[lImageIndex],
                            stRenderArea, stViewport, stScissor, runtimeDrawCalls,
                            vecClearValues.data(), lClearValueCount, preSceneCallback, postSceneCallback);
#endif

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
