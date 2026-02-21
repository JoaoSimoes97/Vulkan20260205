/*
 * ViewportManager — Manages multiple viewports with different cameras and rendering settings.
 * Supports PIP (picture-in-picture) and detachable viewports.
 */
#pragma once

#include "viewport_config.h"
#include "../camera/camera.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <vulkan/vulkan.h>

class SceneNew;
struct VulkanConfig;

/**
 * Viewport render target — offscreen framebuffer for a viewport.
 */
struct ViewportRenderTarget {
    /** Vulkan resources. */
    VkImage colorImage = VK_NULL_HANDLE;
    VkDeviceMemory colorMemory = VK_NULL_HANDLE;
    VkImageView colorView = VK_NULL_HANDLE;
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    
    /** ImGui texture ID for displaying in UI. */
    VkDescriptorSet imguiTextureId = VK_NULL_HANDLE;
    
    /** Current size. */
    uint32_t width = 0;
    uint32_t height = 0;
    
    /** Is this target valid? */
    bool IsValid() const { return framebuffer != VK_NULL_HANDLE; }
};

/**
 * Viewport instance — runtime state for a viewport.
 */
struct Viewport {
    /** Configuration. */
    ViewportConfig config;
    
    /** Render target (for PIP/detached viewports). */
    ViewportRenderTarget renderTarget;
    
    /** Computed camera for this frame. */
    Camera* pCamera = nullptr;
    
    /** Is hovered by mouse? */
    bool bHovered = false;
    
    /** Is focused? */
    bool bFocused = false;
};

/**
 * ViewportManager — Creates and manages viewports.
 */
class ViewportManager {
public:
    ViewportManager() = default;
    ~ViewportManager();
    
    /** Initialize Vulkan resources for viewport rendering. */
    void Create(VkDevice device, VkPhysicalDevice physicalDevice, 
                VkRenderPass renderPass, VkDescriptorPool imguiDescriptorPool,
                VkFormat colorFormat, VkFormat depthFormat,
                uint32_t initialWidth = 1280, uint32_t initialHeight = 720);
    
    /** Destroy resources. */
    void Destroy();
    
    /** Add a new viewport with the given configuration. Returns viewport ID. */
    uint32_t AddViewport(const ViewportConfig& config);
    
    /** Remove a viewport by ID. */
    void RemoveViewport(uint32_t id);
    
    /** Get viewport by ID. Returns nullptr if not found. */
    Viewport* GetViewport(uint32_t id);
    const Viewport* GetViewport(uint32_t id) const;
    
    /** Get all viewports. */
    const std::vector<Viewport>& GetViewports() const { return m_viewports; }
    std::vector<Viewport>& GetViewports() { return m_viewports; }
    
    /** Get the main viewport (ID 0, or first visible). */
    Viewport* GetMainViewport();
    
    /** Get the main viewport's ImGui texture ID for display. */
    VkDescriptorSet GetMainViewportTextureId() const;
    
    /** Get the main viewport's current render size. */
    void GetMainViewportSize(uint32_t& outWidth, uint32_t& outHeight) const;
    
    /** Resize a viewport's render target. */
    void ResizeViewport(uint32_t id, uint32_t width, uint32_t height);
    
    /** Begin rendering to a viewport. Returns command buffer to use. */
    void BeginViewportRender(uint32_t id, VkCommandBuffer cmd);
    
    /** End rendering to a viewport. */
    void EndViewportRender(uint32_t id, VkCommandBuffer cmd);
    
    /** Get camera for a viewport (from SceneNew camera components or main camera). */
    Camera* GetCameraForViewport(const Viewport& viewport, SceneNew* pScene, Camera* pMainCamera);
    
    /** Get next available viewport ID. */
    uint32_t GetNextId() const { return m_nextId; }
    
    /** Get offscreen render pass (for PIP viewports). */
    VkRenderPass GetOffscreenRenderPass() const { return m_offscreenRenderPass; }
    
private:
    /** Create render target for a viewport. */
    void CreateRenderTarget(ViewportRenderTarget& target, uint32_t width, uint32_t height);
    
    /** Destroy render target. */
    void DestroyRenderTarget(ViewportRenderTarget& target);
    
    /** Create offscreen render pass for PIP viewports. */
    void CreateOffscreenRenderPass();
    
    /** Find memory type. */
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkRenderPass m_offscreenRenderPass = VK_NULL_HANDLE;
    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;
    VkFormat m_colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;
    
    std::vector<Viewport> m_viewports;
    uint32_t m_nextId = 1;  // 0 is reserved for main viewport
    
    /** Cached Camera instances for viewport cameras (keyed by GameObject ID). */
    std::unordered_map<uint32_t, std::unique_ptr<Camera>> m_cameraCache;
};
