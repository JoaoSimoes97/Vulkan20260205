/*
 * ImGuiBase - Common ImGui initialization implementation.
 */
#include "imgui_base.h"
#include "vulkan/vulkan_utils.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <SDL3/SDL.h>

namespace {
    /** ImGui Vulkan error callback (required function pointer for ImGui_ImplVulkan_Init). */
    void CheckVkResult(VkResult r) {
        if (r != VK_SUCCESS) {
            VulkanUtils::LogErr("ImGui Vulkan error: {}", static_cast<int>(r));
        }
    }
}

ImGuiBase::~ImGuiBase() {
    if (m_bInitialized) {
        ShutdownImGui();
    }
}

void ImGuiBase::InitImGui(
    SDL_Window* pWindow,
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    uint32_t graphicsQueueFamily,
    VkQueue graphicsQueue,
    VkRenderPass renderPass,
    uint32_t imageCount,
    bool enableDocking,
    bool enableViewports
) {
    if (m_bInitialized) {
        VulkanUtils::LogWarn("ImGuiBase already initialized");
        return;
    }

    m_device = device;

    // Create descriptor pool for ImGui
    CreateDescriptorPool();

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Enable docking if requested (editor needs this)
    if (enableDocking) {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }
    
    // Style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    // Initialize SDL3 backend FIRST - sets BackendFlags based on video driver
    ImGui_ImplSDL3_InitForVulkan(pWindow);
    
    // Multi-viewport: only enable if requested AND platform supports it
    if (enableViewports && (io.BackendFlags & ImGuiBackendFlags_PlatformHasViewports)) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        m_bViewportsEnabled = true;
        VulkanUtils::LogInfo("Multi-viewport enabled");
    } else if (enableViewports) {
        VulkanUtils::LogInfo("Multi-viewport requested but not supported by video driver");
    }
    
    // Update style for viewports if enabled
    if (m_bViewportsEnabled) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 0.95f;
    }

    // Initialize Vulkan backend
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_0;
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = graphicsQueueFamily;
    initInfo.Queue = graphicsQueue;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_descriptorPool;
    initInfo.RenderPass = renderPass;
    initInfo.Subpass = 0;
    initInfo.MinImageCount = imageCount;
    initInfo.ImageCount = imageCount;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = CheckVkResult;

    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();

    m_bInitialized = true;
    VulkanUtils::LogInfo("ImGuiBase initialized (docking: {}, viewports: {})",
        enableDocking ? "on" : "off",
        m_bViewportsEnabled ? "on" : "off");
}

void ImGuiBase::ShutdownImGui() {
    if (!m_bInitialized) return;

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    m_bInitialized = false;
    VulkanUtils::LogInfo("ImGuiBase shutdown");
}

void ImGuiBase::CreateDescriptorPool() {
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = static_cast<uint32_t>(sizeof(poolSizes) / sizeof(poolSizes[0]));
    poolInfo.pPoolSizes = poolSizes;

    VkResult r = vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("Failed to create ImGui descriptor pool: {}", static_cast<int>(r));
    }
}

void ImGuiBase::BeginFrame() {
    if (!m_bInitialized || !m_bEnabled) return;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiBase::EndFrame() {
    if (!m_bInitialized || !m_bEnabled) return;

    ImGui::Render();

    // Update and render additional platform windows (multi-viewport)
    if (m_bViewportsEnabled) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void ImGuiBase::RenderDrawData(VkCommandBuffer commandBuffer) {
    if (!m_bInitialized || !m_bEnabled) return;

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData) {
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
    }
}

bool ImGuiBase::ProcessEvent(const SDL_Event* pEvent) {
    if (!m_bInitialized) return false;
    return ImGui_ImplSDL3_ProcessEvent(pEvent);
}

void ImGuiBase::OnSwapchainRecreate(VkRenderPass renderPass, uint32_t imageCount) {
    (void)renderPass;
    (void)imageCount;
}

bool ImGuiBase::WantCaptureMouse() const {
    if (!m_bInitialized) return false;
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiBase::WantCaptureKeyboard() const {
    if (!m_bInitialized) return false;
    return ImGui::GetIO().WantCaptureKeyboard;
}
