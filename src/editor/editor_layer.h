/*
 * EditorLayer — ImGui-based editor overlay.
 * Only active in Editor/Debug builds. Provides object selection, transform gizmos,
 * hierarchy panel, inspector, and multi-viewport support.
 */
#pragma once

#if EDITOR_BUILD  // Editor only in Debug/Editor builds

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

struct SDL_Window;
class SceneNew;
class Scene;
class Camera;
struct Transform;
class LevelSelector;
struct VulkanConfig;
class ViewportManager;

/**
 * Gizmo operation mode.
 */
enum class GizmoOperation {
    None,
    Translate,
    Rotate,
    Scale
};

/**
 * Gizmo coordinate space.
 */
enum class GizmoSpace {
    World,
    Local
};

/**
 * EditorLayer — ImGui integration for visual editing.
 * 
 * Features:
 * - Object selection via click (ray casting)
 * - Transform gizmos (translate, rotate, scale)
 * - Hierarchy panel (scene tree)
 * - Inspector panel (edit properties)
 * - Multi-viewport docking support
 */
class EditorLayer {
public:
    EditorLayer() = default;
    ~EditorLayer();

    // Non-copyable
    EditorLayer(const EditorLayer&) = delete;
    EditorLayer& operator=(const EditorLayer&) = delete;

    /**
     * Initialize ImGui with Vulkan and SDL3.
     * Call after Vulkan device and window are created.
     * @param layoutPath Path to ImGui layout ini file (from config).
     */
    void Init(
        SDL_Window* pWindow,
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        uint32_t graphicsQueueFamily,
        VkQueue graphicsQueue,
        VkRenderPass renderPass,
        uint32_t imageCount,
        const std::string& layoutPath = "config/imgui_layout.ini"
    );

    /**
     * Shutdown ImGui and free resources.
     */
    void Shutdown();

    /**
     * Begin new ImGui frame. Call before any ImGui rendering.
     */
    void BeginFrame();

    /**
     * Draw editor panels and gizmos.
     * Call after BeginFrame(), before EndFrame().
     */
    void DrawEditor(SceneNew* pScene, Camera* pCamera, const VulkanConfig& config, ViewportManager* pViewportManager = nullptr, Scene* pRenderScene = nullptr);

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
     * Returns true if ImGui wants the event (don't pass to scene).
     */
    bool ProcessEvent(const void* pEvent);

    /**
     * Called when swapchain is recreated (resize).
     */
    void OnSwapchainRecreate(VkRenderPass renderPass, uint32_t imageCount);

    /* ---- Selection ---- */

    /** Set selected GameObject by ID. UINT32_MAX to deselect. */
    void SetSelectedObject(uint32_t gameObjectId);

    /** Get currently selected GameObject ID. UINT32_MAX if none. */
    uint32_t GetSelectedObject() const { return m_selectedObjectId; }

    /** Perform ray cast selection from screen position. */
    void SelectAtScreenPos(SceneNew* pScene, Camera* pCamera, float screenX, float screenY, uint32_t viewportW, uint32_t viewportH);

    /* ---- Gizmo ---- */

    /** Set gizmo operation mode. */
    void SetGizmoOperation(GizmoOperation op) { m_gizmoOperation = op; }
    GizmoOperation GetGizmoOperation() const { return m_gizmoOperation; }

    /** Set gizmo coordinate space. */
    void SetGizmoSpace(GizmoSpace space) { m_gizmoSpace = space; }
    GizmoSpace GetGizmoSpace() const { return m_gizmoSpace; }

    /** Check if gizmo is currently being manipulated. */
    bool IsGizmoUsing() const { return m_bGizmoUsing; }

    /* ---- State ---- */

    /** Check if editor is initialized. */
    bool IsInitialized() const { return m_bInitialized; }

    /** Enable/disable editor rendering (toggle). */
    void SetEnabled(bool b) { m_bEnabled = b; }
    bool IsEnabled() const { return m_bEnabled; }

    /** Set the current level file path (for save functionality). */
    void SetLevelPath(const std::string& path) { m_currentLevelPath = path; }
    const std::string& GetLevelPath() const { return m_currentLevelPath; }

    /* ---- Level Loading ---- */

    /** Set level selector for File > Load Level menu. */
    void SetLevelSelector(LevelSelector* pSelector) { m_pLevelSelector = pSelector; }

    /** Set callback for unloading the current scene. */
    void SetUnloadSceneCallback(std::function<void()> callback) { m_unloadSceneCallback = std::move(callback); }

    /** Check if a level load was requested (and clear the flag). */
    bool ConsumeLoadRequest();

    /** Check if ImGui wants mouse input. */
    bool WantCaptureMouse() const;

    /** Check if ImGui wants keyboard input. */
    bool WantCaptureKeyboard() const;

private:
    void DrawHierarchyPanel(SceneNew* pScene);
    void DrawInspectorPanel(SceneNew* pScene);
    void DrawViewportPanel(SceneNew* pScene, Camera* pCamera, const VulkanConfig& config, ViewportManager* pViewportManager);
    void DrawViewportsPanel(ViewportManager* pViewportManager, SceneNew* pScene);
    void DrawCamerasPanel(SceneNew* pScene);
    void DrawGizmo(SceneNew* pScene, Camera* pCamera);
    void DrawToolbar();
    void DrawMenuBar();
    void DrawFileMenu();
    void DrawEditMenu();
    void DrawSelectionMenu(SceneNew* pScene);
    void DrawViewMenu();
    void DrawLayoutMenu();
    void DrawHelpMenu();
    
    /** Helper to draw a placeholder menu item in red (not yet implemented). */
    bool PlaceholderMenuItem(const char* label, const char* shortcut = nullptr);

    /** Create descriptor pool for ImGui. */
    void CreateDescriptorPool();

    /** Save modified scene/level to disk. */
    void SaveCurrentLevel(SceneNew* pScene);
    
    /** Reset layout to default. */
    void ResetLayoutToDefault();

    bool m_bInitialized = false;
    bool m_bEnabled = true;
    bool m_bGizmoUsing = false;

    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

    uint32_t m_selectedObjectId = UINT32_MAX;
    GizmoOperation m_gizmoOperation = GizmoOperation::Translate;
    GizmoSpace m_gizmoSpace = GizmoSpace::World;

    // Cached transforms before gizmo edit (for undo)
    float m_cachedPosition[3] = {0.f, 0.f, 0.f};
    float m_cachedRotation[4] = {0.f, 0.f, 0.f, 1.f};
    float m_cachedScale[3] = {1.f, 1.f, 1.f};

    // Level path for saving
    std::string m_currentLevelPath;
    
    // Editor layout ini file path
    std::string m_layoutFilePath = "config/imgui_layout.ini";
    
    // Track if main viewport is hovered (for camera input bypass)
    bool m_bViewportHovered = false;
    
    // Viewport bounds for gizmo positioning (content region, not window)
    float m_viewportX = 0.f;
    float m_viewportY = 0.f;
    float m_viewportW = 0.f;
    float m_viewportH = 0.f;
    
    // Render Scene for emissive light editing (Objects with emitsLight)
    Scene* m_pRenderScene = nullptr;
    
    // Level selector and callbacks
    LevelSelector* m_pLevelSelector = nullptr;
    std::function<void()> m_unloadSceneCallback;
    
    // Panel visibility toggles
    bool m_bShowHierarchy = true;
    bool m_bShowInspector = true;
    bool m_bShowToolbar = true;
    bool m_bShowViewport = true;
    bool m_bShowViewports = true;
    bool m_bShowCameras = true;
    bool m_bShowDemo = false;  // ImGui demo window
};

#endif // EDITOR_BUILD
