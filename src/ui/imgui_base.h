/*
 * ImGuiBase - Common ImGui initialization for all UI overlays.
 * Shared between EditorLayer (Debug) and RuntimeOverlay (Release).
 * 
 * This class handles:
 * - ImGui context creation/destruction
 * - Vulkan backend initialization
 * - SDL3 backend initialization
 * - Descriptor pool management
 * - Frame begin/end and rendering
 * 
 * Subclasses implement DrawContent() to render their specific UI.
 */
#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

struct SDL_Window;
union SDL_Event;

/**
 * ImGuiBase - Base class for ImGui-based UI overlays.
 * 
 * Provides common ImGui initialization and rendering infrastructure.
 * Derived classes override DrawContent() to render their specific UI.
 */
class ImGuiBase {
public:
    ImGuiBase() = default;
    virtual ~ImGuiBase();

    // Non-copyable
    ImGuiBase(const ImGuiBase&) = delete;
    ImGuiBase& operator=(const ImGuiBase&) = delete;

    /**
     * Initialize ImGui with Vulkan and SDL3.
     * @param enableDocking Enable ImGui docking feature (editor needs this).
     * @param enableViewports Enable multi-viewport (windows outside main window).
     */
    void InitImGui(
        SDL_Window* pWindow,
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        uint32_t graphicsQueueFamily,
        VkQueue graphicsQueue,
        VkRenderPass renderPass,
        uint32_t imageCount,
        bool enableDocking = false,
        bool enableViewports = false
    );

    /**
     * Shutdown ImGui and free resources.
     */
    void ShutdownImGui();

    /**
     * Begin new ImGui frame. Call before any ImGui rendering.
     */
    void BeginFrame();

    /**
     * End ImGui frame and prepare for rendering.
     */
    void EndFrame();

    /**
     * Record ImGui draw commands into the command buffer.
     * Call after EndFrame(), during render pass.
     */
    void RenderDrawData(VkCommandBuffer commandBuffer);

    /**
     * Handle SDL event for ImGui input.
     * Returns true if ImGui wants the event.
     */
    bool ProcessEvent(const SDL_Event* pEvent);

    /**
     * Called when swapchain is recreated.
     */
    void OnSwapchainRecreate(VkRenderPass renderPass, uint32_t imageCount);

    /**
     * Check if ImGui is initialized.
     */
    bool IsInitialized() const { return m_bInitialized; }

    /**
     * Enable/disable rendering.
     */
    void SetEnabled(bool b) { m_bEnabled = b; }
    bool IsEnabled() const { return m_bEnabled; }

    /**
     * Check if ImGui wants mouse input.
     */
    bool WantCaptureMouse() const;

    /**
     * Check if ImGui wants keyboard input.
     */
    bool WantCaptureKeyboard() const;

protected:
    /**
     * Override to draw UI content. Called between BeginFrame() and EndFrame().
     */
    virtual void DrawContent() = 0;

    VkDevice GetDevice() const { return m_device; }

private:
    void CreateDescriptorPool();

    bool m_bInitialized = false;
    bool m_bEnabled = true;
    bool m_bViewportsEnabled = false;
    bool m_bOwnsContext = false;  // True if this instance created the shared context

    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    
    // Static: shared ImGui context management
    static int s_contextRefCount;
    static bool s_backendsInitialized;
};
