/*
 * EditorLayer — ImGui-based editor overlay implementation.
 */
#if EDITOR_BUILD  // Editor only in Debug/Editor builds

#include "editor_layer.h"
#include "core/scene_new.h"
#include "core/transform.h"
#include "core/renderer_component.h"
#include "core/camera_component.h"
#include "managers/mesh_manager.h"
#include "scene/scene.h"
#include "scene/object.h"
#include "camera/camera.h"
#include "config/vulkan_config.h"
#include "render/viewport_config.h"
#include "render/viewport_manager.h"
#include "vulkan/vulkan_utils.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <ImGuizmo.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>

#include <SDL3/SDL.h>
#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>

namespace {
    /** ImGui Vulkan error callback (required function pointer for ImGui_ImplVulkan_Init). */
    void CheckVkResult(VkResult r) {
        if (r != VK_SUCCESS) {
            VulkanUtils::LogErr("ImGui Vulkan error: {}", static_cast<int>(r));
        }
    }
}

EditorLayer::~EditorLayer() {
    if (m_bInitialized) {
        Shutdown();
    }
}

void EditorLayer::Init(
    SDL_Window* pWindow,
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    uint32_t graphicsQueueFamily,
    VkQueue graphicsQueue,
    VkRenderPass renderPass,
    uint32_t imageCount,
    const std::string& layoutPath
) {
    if (m_bInitialized) {
        VulkanUtils::LogWarn("EditorLayer already initialized");
        return;
    }

    m_device = device;
    m_layoutFilePath = layoutPath;

    // Create descriptor pool for ImGui
    CreateDescriptorPool();

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Enable docking (works on all platforms)
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    // Disable ImGui auto-save (user controls save via Layout menu)
    io.IniFilename = nullptr;
    
    // Try to load saved layout if it exists
    if (!m_layoutFilePath.empty()) {
        ImGui::LoadIniSettingsFromDisk(m_layoutFilePath.c_str());
        VulkanUtils::LogInfo("Editor layout path: {}", m_layoutFilePath);
    }
    
    // Style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    // Initialize SDL3 backend FIRST - this sets up BackendFlags based on video driver
    // (windows/cocoa/x11 support viewports, wayland does not yet)
    ImGui_ImplSDL3_InitForVulkan(pWindow);
    
    // Multi-viewport: only enable if the platform backend supports it
    // SDL3 sets ImGuiBackendFlags_PlatformHasViewports based on video driver
    // (supported: windows, cocoa, x11; not supported: wayland)
    if (io.BackendFlags & ImGuiBackendFlags_PlatformHasViewports) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        VulkanUtils::LogInfo("Multi-viewport enabled (video driver supports it)");
    } else {
        VulkanUtils::LogInfo("Multi-viewport disabled (video driver: {} does not support global mouse state)",
            SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown");
    }
    
    // Update style for viewports if enabled
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        // Keep window backgrounds slightly transparent for non-dockspace windows
        style.Colors[ImGuiCol_WindowBg].w = 0.95f;
    }

    // Initialize Vulkan backend
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_0;  // Required in newer ImGui versions
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

    // Note: ImGui_ImplVulkan_CreateFontsTexture() is now automatically called
    // by ImGui_ImplVulkan_NewFrame() on first use (since imgui 1.91+).
    // Calling it explicitly here is still supported for compatibility.
    ImGui_ImplVulkan_CreateFontsTexture();

    m_bInitialized = true;
    VulkanUtils::LogInfo("EditorLayer initialized (docking: enabled, viewports: {})",
        (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) ? "enabled" : "disabled");
}

void EditorLayer::Shutdown() {
    if (!m_bInitialized) return;

    // Note: Auto-save disabled. User saves layout via Layout menu.
    
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    m_bInitialized = false;
    VulkanUtils::LogInfo("EditorLayer shutdown");
}

void EditorLayer::CreateDescriptorPool() {
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

void EditorLayer::BeginFrame() {
    if (!m_bInitialized || !m_bEnabled) return;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void EditorLayer::EndFrame() {
    if (!m_bInitialized || !m_bEnabled) return;

    ImGui::Render();

    // Update and render additional platform windows (multi-viewport)
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void EditorLayer::RenderDrawData(VkCommandBuffer commandBuffer) {
    if (!m_bInitialized || !m_bEnabled) return;

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData) {
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
    }
}

bool EditorLayer::ProcessEvent(const void* pEvent) {
    if (!m_bInitialized) return false;
    return ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(pEvent));
}

void EditorLayer::OnSwapchainRecreate(VkRenderPass renderPass, uint32_t imageCount) {
    // ImGui Vulkan backend handles this automatically through SetMinImageCount if needed
    (void)renderPass;
    (void)imageCount;
}

bool EditorLayer::WantCaptureMouse() const {
    if (!m_bInitialized) return false;
    
    // If gizmo is being used, editor always wants the mouse
    if (m_bGizmoUsing) return true;
    
    // If viewport is hovered, allow camera input (editor doesn't need mouse)
    if (m_bViewportHovered) return false;
    
    return ImGui::GetIO().WantCaptureMouse;
}

bool EditorLayer::WantCaptureKeyboard() const {
    if (!m_bInitialized) return false;
    return ImGui::GetIO().WantCaptureKeyboard;
}

void EditorLayer::DrawEditor(SceneNew* pScene, Camera* pCamera, const VulkanConfig& config, ViewportManager* pViewportManager, Scene* pRenderScene) {
    if (!m_bInitialized || !m_bEnabled || !pScene) return;
    
    // Store render scene for inspector access (emissive light editing)
    m_pRenderScene = pRenderScene;

    // Setup dockspace over the entire viewport
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags dockspaceFlags = 
        ImGuiWindowFlags_MenuBar | 
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    // Transparent background for pass-through central node
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    
    ImGui::Begin("DockSpace", nullptr, dockspaceFlags);
    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(3);

    // Create the dockspace
    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    // Menu bar
    DrawMenuBar();

    ImGui::End();

    // Draw panels (based on visibility toggles)
    if (m_bShowToolbar) DrawToolbar();
    if (m_bShowHierarchy) DrawHierarchyPanel(pScene);
    if (m_bShowInspector) DrawInspectorPanel(pScene);
    if (m_bShowCameras) DrawCamerasPanel(pScene);
    if (m_bShowViewport) DrawViewportPanel(pScene, pCamera, config, pViewportManager);
    if (m_bShowViewports) DrawViewportsPanel(pViewportManager, pScene);
    if (m_bShowDemo) ImGui::ShowDemoWindow(&m_bShowDemo);
}

// Helper to draw a placeholder menu item in red (not yet implemented)
bool EditorLayer::PlaceholderMenuItem(const char* label, const char* shortcut) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));  // Red text
    bool clicked = ImGui::MenuItem(label, shortcut);
    ImGui::PopStyleColor();
    return clicked;
}

void EditorLayer::ResetLayoutToDefault() {
    // Clear current docking layout by loading empty settings
    // This will cause ImGui to rebuild default undocked layout on next frame
    ImGui::LoadIniSettingsFromMemory("");
    
    VulkanUtils::LogInfo("Layout reset to default - panels undocked");
}

void EditorLayer::DrawMenuBar() {
    if (ImGui::BeginMenuBar()) {
        DrawFileMenu();
        DrawEditMenu();
        DrawSelectionMenu(nullptr);  // TODO: pass scene
        DrawViewMenu();
        DrawLayoutMenu();
        DrawHelpMenu();
        ImGui::EndMenuBar();
    }
}

void EditorLayer::DrawFileMenu() {
    if (ImGui::BeginMenu("File")) {
        PlaceholderMenuItem("New Level", "Ctrl+N");
        PlaceholderMenuItem("Open Level...", "Ctrl+O");
        PlaceholderMenuItem("Open Recent");
        ImGui::Separator();
        if (ImGui::MenuItem("Save Level", "Ctrl+S")) {
            // SaveCurrentLevel will be called - already partially implemented
        }
        PlaceholderMenuItem("Save Level As...", "Ctrl+Shift+S");
        ImGui::Separator();
        PlaceholderMenuItem("Import...");
        PlaceholderMenuItem("Export...");
        ImGui::Separator();
        PlaceholderMenuItem("Project Settings...");
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
            // Request window close - placeholder for now
        }
        ImGui::EndMenu();
    }
}

void EditorLayer::DrawEditMenu() {
    if (ImGui::BeginMenu("Edit")) {
        PlaceholderMenuItem("Undo", "Ctrl+Z");
        PlaceholderMenuItem("Redo", "Ctrl+Y");
        ImGui::Separator();
        PlaceholderMenuItem("Cut", "Ctrl+X");
        PlaceholderMenuItem("Copy", "Ctrl+C");
        PlaceholderMenuItem("Paste", "Ctrl+V");
        PlaceholderMenuItem("Duplicate", "Ctrl+D");
        PlaceholderMenuItem("Delete", "Del");
        ImGui::Separator();
        PlaceholderMenuItem("Select All", "Ctrl+A");
        PlaceholderMenuItem("Deselect All", "Ctrl+Shift+A");
        ImGui::Separator();
        PlaceholderMenuItem("Preferences...");
        ImGui::EndMenu();
    }
}

void EditorLayer::DrawSelectionMenu(SceneNew* pScene) {
    (void)pScene;  // TODO: use for operations
    if (ImGui::BeginMenu("Selection")) {
        PlaceholderMenuItem("Select All", "Ctrl+A");
        PlaceholderMenuItem("Deselect All", "Ctrl+Shift+A");
        PlaceholderMenuItem("Invert Selection");
        ImGui::Separator();
        PlaceholderMenuItem("Select Parent");
        PlaceholderMenuItem("Select Children");
        ImGui::Separator();
        PlaceholderMenuItem("Select by Type...");
        PlaceholderMenuItem("Select by Name...");
        ImGui::EndMenu();
    }
}

void EditorLayer::DrawViewMenu() {
    if (ImGui::BeginMenu("View")) {
        ImGui::Text("Panels:");
        ImGui::Separator();
        ImGui::MenuItem("Toolbar", nullptr, &m_bShowToolbar);
        ImGui::MenuItem("Hierarchy", nullptr, &m_bShowHierarchy);
        ImGui::MenuItem("Inspector", nullptr, &m_bShowInspector);
        ImGui::MenuItem("Viewport", nullptr, &m_bShowViewport);
        ImGui::MenuItem("Viewports Manager", nullptr, &m_bShowViewports);
        ImGui::MenuItem("Cameras", nullptr, &m_bShowCameras);
        ImGui::Separator();
        ImGui::MenuItem("ImGui Demo", nullptr, &m_bShowDemo);
        ImGui::Separator();
        PlaceholderMenuItem("Console");
        PlaceholderMenuItem("Profiler");
        PlaceholderMenuItem("Asset Browser");
        ImGui::Separator();
        
        if (ImGui::BeginMenu("Gizmo")) {
            if (ImGui::MenuItem("Translate", "W", m_gizmoOperation == GizmoOperation::Translate)) {
                m_gizmoOperation = GizmoOperation::Translate;
            }
            if (ImGui::MenuItem("Rotate", "E", m_gizmoOperation == GizmoOperation::Rotate)) {
                m_gizmoOperation = GizmoOperation::Rotate;
            }
            if (ImGui::MenuItem("Scale", "R", m_gizmoOperation == GizmoOperation::Scale)) {
                m_gizmoOperation = GizmoOperation::Scale;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("World Space", nullptr, m_gizmoSpace == GizmoSpace::World)) {
                m_gizmoSpace = GizmoSpace::World;
            }
            if (ImGui::MenuItem("Local Space", nullptr, m_gizmoSpace == GizmoSpace::Local)) {
                m_gizmoSpace = GizmoSpace::Local;
            }
            ImGui::EndMenu();
        }
        
        ImGui::Separator();
        PlaceholderMenuItem("Fullscreen", "F11");
        ImGui::EndMenu();
    }
}

void EditorLayer::DrawLayoutMenu() {
    if (ImGui::BeginMenu("Layout")) {
        if (ImGui::MenuItem("Save Layout")) {
            if (!m_layoutFilePath.empty()) {
                ImGui::SaveIniSettingsToDisk(m_layoutFilePath.c_str());
                VulkanUtils::LogInfo("Layout saved to {}", m_layoutFilePath);
            }
        }
        if (ImGui::MenuItem("Load Layout")) {
            if (!m_layoutFilePath.empty()) {
                ImGui::LoadIniSettingsFromDisk(m_layoutFilePath.c_str());
                VulkanUtils::LogInfo("Layout loaded from {}", m_layoutFilePath);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset to Default")) {
            ResetLayoutToDefault();
        }
        ImGui::Separator();
        PlaceholderMenuItem("Save Layout As...");
        PlaceholderMenuItem("Load Layout from File...");
        ImGui::Separator();
        ImGui::Text("Presets:");
        PlaceholderMenuItem("  Default");
        PlaceholderMenuItem("  Wide");
        PlaceholderMenuItem("  Tall");
        PlaceholderMenuItem("  Dual Monitor");
        ImGui::EndMenu();
    }
}

void EditorLayer::DrawHelpMenu() {
    if (ImGui::BeginMenu("Help")) {
        PlaceholderMenuItem("Documentation");
        PlaceholderMenuItem("Keyboard Shortcuts");
        ImGui::Separator();
        PlaceholderMenuItem("Report a Bug...");
        PlaceholderMenuItem("Check for Updates");
        ImGui::Separator();
        if (ImGui::MenuItem("About")) {
            // Could show a popup with version info
        }
        ImGui::EndMenu();
    }
}

void EditorLayer::DrawToolbar() {
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Gizmo operation buttons
    bool isTranslate = (m_gizmoOperation == GizmoOperation::Translate);
    bool isRotate = (m_gizmoOperation == GizmoOperation::Rotate);
    bool isScale = (m_gizmoOperation == GizmoOperation::Scale);

    if (isTranslate) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button("Translate (W)")) m_gizmoOperation = GizmoOperation::Translate;
    if (isTranslate) ImGui::PopStyleColor();

    ImGui::SameLine();
    if (isRotate) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button("Rotate (E)")) m_gizmoOperation = GizmoOperation::Rotate;
    if (isRotate) ImGui::PopStyleColor();

    ImGui::SameLine();
    if (isScale) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button("Scale (R)")) m_gizmoOperation = GizmoOperation::Scale;
    if (isScale) ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Space toggle
    bool isWorld = (m_gizmoSpace == GizmoSpace::World);
    if (ImGui::Button(isWorld ? "World" : "Local")) {
        m_gizmoSpace = isWorld ? GizmoSpace::Local : GizmoSpace::World;
    }

    // Handle keyboard shortcuts (only when not hovered over viewport - conflict with camera movement)
    if (!ImGui::GetIO().WantCaptureKeyboard && !m_bViewportHovered) {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) m_gizmoOperation = GizmoOperation::Translate;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) m_gizmoOperation = GizmoOperation::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmoOperation = GizmoOperation::Scale;
    }

    ImGui::End();
}

void EditorLayer::DrawHierarchyPanel(SceneNew* pScene) {
    ImGui::Begin("Hierarchy");

    if (pScene) {
        // Get root objects (those without parents)
        std::vector<uint32_t> roots = pScene->GetRootObjects();
        
        // "[Root]" drop target for unparenting objects
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::Selectable("## [Drop here to unparent]", false, ImGuiSelectableFlags_None);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("[Root - Drop here to unparent]");
        
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT_ID")) {
                uint32_t draggedId = *static_cast<const uint32_t*>(payload->Data);
                pScene->SetParent(draggedId, NO_PARENT);
            }
            ImGui::EndDragDropTarget();
        }
        
        ImGui::Separator();
        
        // Recursive lambda to draw tree nodes
        std::function<void(uint32_t)> drawNode = [&](uint32_t goId) {
            const GameObject* pGO = pScene->FindGameObject(goId);
            if (!pGO || !pGO->bActive) return;
            
            bool hasChildren = !pGO->children.empty();
            
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (goId == m_selectedObjectId) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            if (!hasChildren) {
                flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            }
            
            bool nodeOpen = ImGui::TreeNodeEx(
                reinterpret_cast<void*>(static_cast<intptr_t>(goId)),
                flags,
                "%s [%u]",
                pGO->name.empty() ? "Unnamed" : pGO->name.c_str(),
                goId
            );
            
            // Selection
            if (ImGui::IsItemClicked()) {
                SetSelectedObject(goId);
            }
            
            // Drag source for reparenting
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("GAMEOBJECT_ID", &goId, sizeof(uint32_t));
                ImGui::Text("Move: %s", pGO->name.empty() ? "Unnamed" : pGO->name.c_str());
                ImGui::EndDragDropSource();
            }
            
            // Drop target for reparenting
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT_ID")) {
                    uint32_t draggedId = *static_cast<const uint32_t*>(payload->Data);
                    if (draggedId != goId) {
                        pScene->SetParent(draggedId, goId);
                    }
                }
                ImGui::EndDragDropTarget();
            }
            
            // Draw children if node is open
            if (nodeOpen && hasChildren) {
                for (uint32_t childId : pGO->children) {
                    drawNode(childId);
                }
                ImGui::TreePop();
            }
        };
        
        // Draw root objects
        for (uint32_t rootId : roots) {
            drawNode(rootId);
        }
    }

    ImGui::End();
}

void EditorLayer::DrawInspectorPanel(SceneNew* pScene) {
    ImGui::Begin("Inspector");

    if (pScene && m_selectedObjectId != UINT32_MAX) {
        GameObject* pGO = pScene->FindGameObject(m_selectedObjectId);
        if (pGO) {
            // Name
            char nameBuf[256];
            std::snprintf(nameBuf, sizeof(nameBuf), "%s", pGO->name.c_str());
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                pGO->name = nameBuf;
            }

            ImGui::Separator();

            // Transform
            Transform* pTransform = pScene->GetTransform(m_selectedObjectId);
            if (pTransform && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                bool changed = false;
                
                // Parent assignment
                {
                    uint32_t currentParent = pTransform->parentId;
                    std::string currentParentName = "(None - Root)";
                    if (currentParent != NO_PARENT) {
                        const GameObject* pParentGO = pScene->FindGameObject(currentParent);
                        if (pParentGO) {
                            currentParentName = pParentGO->name.empty() ? "Unnamed" : pParentGO->name;
                            currentParentName += " [" + std::to_string(currentParent) + "]";
                        }
                    }
                    
                    if (ImGui::BeginCombo("Parent", currentParentName.c_str())) {
                        // Option to clear parent
                        if (ImGui::Selectable("(None - Root)", currentParent == NO_PARENT)) {
                            pScene->SetParent(m_selectedObjectId, NO_PARENT);
                            changed = true;
                        }
                        
                        // List all other objects as potential parents
                        const auto& gameObjects = pScene->GetGameObjects();
                        for (const auto& go : gameObjects) {
                            if (!go.bActive) continue;
                            if (go.id == m_selectedObjectId) continue; // Can't parent to self
                            if (pScene->WouldCreateCycle(m_selectedObjectId, go.id)) continue; // Skip cycles
                            
                            std::string label = go.name.empty() ? "Unnamed" : go.name;
                            label += " [" + std::to_string(go.id) + "]";
                            
                            if (ImGui::Selectable(label.c_str(), currentParent == go.id)) {
                                pScene->SetParent(m_selectedObjectId, go.id);
                                changed = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                
                ImGui::Separator();
                ImGui::Text("Local Transform");
                ImGui::Indent();

                // Local Position (editable)
                if (ImGui::DragFloat3("Position##Local", pTransform->position, 0.1f)) {
                    changed = true;
                }

                // Rotation (show as Euler angles)
                glm::quat q(pTransform->rotation[3], pTransform->rotation[0], pTransform->rotation[1], pTransform->rotation[2]);
                glm::vec3 euler = glm::degrees(glm::eulerAngles(q));
                if (ImGui::DragFloat3("Rotation##Local", &euler.x, 1.0f)) {
                    glm::vec3 radians = glm::radians(euler);
                    glm::quat newQ = glm::quat(radians);
                    pTransform->rotation[0] = newQ.x;
                    pTransform->rotation[1] = newQ.y;
                    pTransform->rotation[2] = newQ.z;
                    pTransform->rotation[3] = newQ.w;
                    changed = true;
                }

                // Scale (clamp to prevent zero values)
                if (ImGui::DragFloat3("Scale##Local", pTransform->scale, 0.1f, 0.001f, 100.0f)) {
                    pTransform->scale[0] = std::max(pTransform->scale[0], 0.001f);
                    pTransform->scale[1] = std::max(pTransform->scale[1], 0.001f);
                    pTransform->scale[2] = std::max(pTransform->scale[2], 0.001f);
                    changed = true;
                }
                
                ImGui::Unindent();
                
                // Show World Transform (read-only) if object has a parent
                if (pTransform->HasParent()) {
                    ImGui::Separator();
                    ImGui::Text("World Transform (read-only)");
                    ImGui::Indent();
                    
                    // Extract world position from world matrix
                    float worldPos[3] = { pTransform->worldMatrix[12], pTransform->worldMatrix[13], pTransform->worldMatrix[14] };
                    ImGui::BeginDisabled();
                    ImGui::DragFloat3("Position##World", worldPos);
                    
                    // Extract world rotation from world matrix
                    Transform tempTransform;
                    TransformFromMatrix(pTransform->worldMatrix, tempTransform);
                    glm::quat worldQ(tempTransform.rotation[3], tempTransform.rotation[0], tempTransform.rotation[1], tempTransform.rotation[2]);
                    glm::vec3 worldEuler = glm::degrees(glm::eulerAngles(worldQ));
                    ImGui::DragFloat3("Rotation##World", &worldEuler.x);
                    
                    // Show world scale
                    ImGui::DragFloat3("Scale##World", tempTransform.scale);
                    ImGui::EndDisabled();
                    
                    ImGui::Unindent();
                }

                if (changed) {
                    pTransform->bDirty = true;
                }
            }

            // Light component
            if (pGO->HasLight()) {
                auto& lights = pScene->GetLights();
                if (pGO->lightIndex < lights.size()) {
                    LightComponent& light = lights[pGO->lightIndex];
                    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                        // Light type
                        const char* lightTypes[] = { "Directional", "Point", "Spot" };
                        int currentType = static_cast<int>(light.type);
                        if (ImGui::Combo("Type", &currentType, lightTypes, 3)) {
                            light.type = static_cast<LightType>(currentType);
                        }

                        ImGui::ColorEdit3("Color", light.color);
                        ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);

                        if (light.type == LightType::Point || light.type == LightType::Spot) {
                            ImGui::DragFloat("Range", &light.range, 0.1f, 0.1f, 1000.0f);
                        }

                        if (light.type == LightType::Spot) {
                            float innerDeg = glm::degrees(light.innerConeAngle);
                            float outerDeg = glm::degrees(light.outerConeAngle);
                            if (ImGui::DragFloat("Inner Cone", &innerDeg, 1.0f, 0.0f, 90.0f)) {
                                light.innerConeAngle = glm::radians(innerDeg);
                            }
                            if (ImGui::DragFloat("Outer Cone", &outerDeg, 1.0f, 0.0f, 90.0f)) {
                                light.outerConeAngle = glm::radians(outerDeg);
                            }
                        }
                    }
                }
            }

            // Renderer component info (read-only for now)
            if (pGO->HasRenderer()) {
                if (ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& renderers = pScene->GetRenderers();
                    if (pGO->rendererIndex < renderers.size()) {
                        const RendererComponent& renderer = renderers[pGO->rendererIndex];
                        
                        // Mesh info
                        ImGui::Text("Mesh:");
                        ImGui::Indent();
                        if (renderer.mesh) {
                            ImGui::Text("Vertices: %u", renderer.mesh->GetVertexCount());
                            const auto& aabb = renderer.mesh->GetAABB();
                            if (aabb.IsValid()) {
                                float cx, cy, cz;
                                aabb.GetCenter(cx, cy, cz);
                                ImGui::Text("AABB Center: (%.2f, %.2f, %.2f)", cx, cy, cz);
                                ImGui::Text("AABB Size: (%.2f, %.2f, %.2f)", 
                                    aabb.maxX - aabb.minX, 
                                    aabb.maxY - aabb.minY, 
                                    aabb.maxZ - aabb.minZ);
                            }
                        } else {
                            ImGui::TextDisabled("No mesh assigned");
                        }
                        ImGui::Unindent();
                        
                        // Material properties
                        ImGui::Text("Material:");
                        ImGui::Indent();
                        ImGui::ColorEdit4("Base Color", const_cast<float*>(renderer.matProps.baseColor), ImGuiColorEditFlags_NoInputs);
                        ImGui::Text("Metallic: %.2f", renderer.matProps.metallic);
                        ImGui::Text("Roughness: %.2f", renderer.matProps.roughness);
                        ImGui::ColorEdit3("Emissive", const_cast<float*>(renderer.matProps.emissive), ImGuiColorEditFlags_NoInputs);
                        ImGui::Unindent();
                        
                        // Render state
                        ImGui::Text("Visible: %s", renderer.bVisible ? "Yes" : "No");
                        ImGui::Text("Cast Shadow: %s", renderer.bCastShadow ? "Yes" : "No");
                        ImGui::Text("Layer: %u", static_cast<uint32_t>(renderer.layer));
                    }
                }
            }
            
            // Emissive Light properties (from render Scene Objects)
            // Find the Object in the render scene that corresponds to this GameObject
            if (m_pRenderScene && pGO->HasRenderer()) {
                Object* pObj = nullptr;
                auto& renderObjects = m_pRenderScene->GetObjects();
                for (size_t objIdx = 0; objIdx < renderObjects.size(); ++objIdx) {
                    if (renderObjects[objIdx].gameObjectId == m_selectedObjectId) {
                        pObj = &const_cast<Object&>(renderObjects[objIdx]);
                        break;
                    }
                }
                
                if (pObj) {
                    if (ImGui::CollapsingHeader("Emissive Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::Checkbox("Emits Light", &pObj->emitsLight);
                        
                        if (pObj->emitsLight) {
                            ImGui::ColorEdit3("Light Color", pObj->emissive);
                            ImGui::DragFloat("Emissive Strength", &pObj->emissive[3], 0.1f, 0.0f, 100.0f);
                            ImGui::DragFloat("Light Radius", &pObj->emissiveLightRadius, 0.5f, 0.1f, 100.0f);
                            ImGui::DragFloat("Light Intensity", &pObj->emissiveLightIntensity, 0.1f, 0.0f, 100.0f);
                            
                            ImGui::Separator();
                            ImGui::TextDisabled("Emissive objects create point lights");
                            ImGui::TextDisabled("at their center to illuminate the scene.");
                        }
                    }
                }
            }
        }
    } else {
        ImGui::TextDisabled("No object selected");
    }

    ImGui::End();
}

void EditorLayer::DrawViewportPanel(SceneNew* pScene, Camera* pCamera, [[maybe_unused]] const VulkanConfig& config, ViewportManager* pViewportManager) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    // Use NoScrollbar to fit the image properly
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    
    ImGui::Begin("Viewport", nullptr, windowFlags);

    // Get viewport size
    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    ImVec2 viewportPos = ImGui::GetCursorScreenPos();
    
    // Store viewport bounds for gizmo positioning (content region, not window)
    m_viewportX = viewportPos.x;
    m_viewportY = viewportPos.y;
    m_viewportW = viewportSize.x;
    m_viewportH = viewportSize.y;
    
    // Resize main viewport render target if size changed
    if (pViewportManager) {
        uint32_t currentWidth = 0, currentHeight = 0;
        pViewportManager->GetMainViewportSize(currentWidth, currentHeight);
        
        uint32_t newWidth = static_cast<uint32_t>(viewportSize.x);
        uint32_t newHeight = static_cast<uint32_t>(viewportSize.y);
        
        if (newWidth > 0 && newHeight > 0 && 
            (currentWidth != newWidth || currentHeight != newHeight)) {
            pViewportManager->ResizeViewport(0, newWidth, newHeight);
        }
        
        // Display the rendered scene image
        VkDescriptorSet textureId = pViewportManager->GetMainViewportTextureId();
        if (textureId != VK_NULL_HANDLE) {
            ImGui::Image((ImTextureID)(uintptr_t)textureId, viewportSize);
        }
    }
    
    // Draw gizmo over viewport
    DrawGizmo(pScene, pCamera);

    // Track if viewport is hovered/focused for camera input
    m_bViewportHovered = ImGui::IsWindowHovered();
    bool isFocused = ImGui::IsWindowFocused();
    
    // Update main viewport state
    if (pViewportManager) {
        Viewport* pMainVp = pViewportManager->GetMainViewport();
        if (pMainVp) {
            pMainVp->bHovered = m_bViewportHovered;
            pMainVp->bFocused = isFocused;
        }
    }

    // Handle click selection in viewport (only when not using gizmo)
    if (m_bViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        float relX = mousePos.x - viewportPos.x;
        float relY = mousePos.y - viewportPos.y;
        SelectAtScreenPos(pScene, pCamera, relX, relY, 
            static_cast<uint32_t>(viewportSize.x), 
            static_cast<uint32_t>(viewportSize.y));
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorLayer::DrawGizmo(SceneNew* pScene, Camera* pCamera) {
    if (!pScene || !pCamera || m_selectedObjectId == UINT32_MAX) {
        m_bGizmoUsing = false;
        return;
    }

    Transform* pTransform = pScene->GetTransform(m_selectedObjectId);
    if (!pTransform) {
        m_bGizmoUsing = false;
        return;
    }

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();

    // Use the stored viewport content region bounds (not window bounds)
    ImGuizmo::SetRect(m_viewportX, m_viewportY, m_viewportW, m_viewportH);

    // Get camera matrices
    glm::mat4 view = pCamera->GetViewMatrix();
    glm::mat4 proj = pCamera->GetProjectionMatrix();
    
    // ImGuizmo expects standard OpenGL projection (Y-up), but we use Vulkan Y-flip.
    // Undo the Y-flip for proper gizmo behavior.
    proj[1][1] = -proj[1][1];
    
    // Fix aspect ratio: the camera projection may use the render target's aspect ratio,
    // which can differ from the ImGui viewport's aspect ratio (especially during resize).
    // Recompute the horizontal scale to match the viewport's actual aspect ratio.
    if (m_viewportW > 0.f && m_viewportH > 0.f) {
        float viewportAspect = m_viewportW / m_viewportH;
        // proj[0][0] = 1 / (aspect * tan(fov/2)), proj[1][1] = 1 / tan(fov/2)
        // So proj[0][0] = proj[1][1] / aspect
        proj[0][0] = proj[1][1] / viewportAspect;
    }

    // Get object model matrix from the RENDER scene (Object.localTransform)
    // This ensures the gizmo is positioned exactly where the object is rendered.
    // The ECS Transform may be out of sync initially - we use the actual render transform.
    glm::mat4 model = glm::mat4(1.0f);
    if (m_pRenderScene) {
        auto& objs = m_pRenderScene->GetObjects();
        for (const auto& obj : objs) {
            if (obj.gameObjectId == m_selectedObjectId) {
                const float* m = obj.localTransform;
                model = glm::mat4(
                    m[0], m[1], m[2], m[3],
                    m[4], m[5], m[6], m[7],
                    m[8], m[9], m[10], m[11],
                    m[12], m[13], m[14], m[15]
                );
                break;
            }
        }
    } else {
        // Fallback to ECS transform if render scene not available
        TransformBuildModelMatrix(*pTransform);
        model = glm::make_mat4(pTransform->modelMatrix);
    }

    // Convert gizmo operation
    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    switch (m_gizmoOperation) {
        case GizmoOperation::Translate: op = ImGuizmo::TRANSLATE; break;
        case GizmoOperation::Rotate: op = ImGuizmo::ROTATE; break;
        case GizmoOperation::Scale: op = ImGuizmo::SCALE; break;
        default: break;
    }

    ImGuizmo::MODE mode = (m_gizmoSpace == GizmoSpace::World) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

    // Manipulate
    bool wasUsing = m_bGizmoUsing;
    if (ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(proj),
        op,
        mode,
        glm::value_ptr(model)
    )) {
        // The gizmo modifies the WORLD matrix. For objects with parents,
        // we need to convert back to LOCAL space.
        glm::mat4 localMatrix = model;
        
        if (pTransform->HasParent()) {
            // Get parent's world matrix
            Transform* pParentTransform = pScene->GetTransform(pTransform->parentId);
            if (pParentTransform) {
                glm::mat4 parentWorld = glm::make_mat4(pParentTransform->worldMatrix);
                glm::mat4 parentWorldInv = glm::inverse(parentWorld);
                // localMatrix = parentWorldInverse * newWorldMatrix
                localMatrix = parentWorldInv * model;
            }
        }
        
        // Check scale from column lengths BEFORE decomposing
        // This prevents singular matrix issues
        const float minScale = 0.01f;
        float scaleX = glm::length(glm::vec3(localMatrix[0]));
        float scaleY = glm::length(glm::vec3(localMatrix[1]));
        float scaleZ = glm::length(glm::vec3(localMatrix[2]));
        
        // If any scale is too small, reject this gizmo operation
        if (scaleX < minScale || scaleY < minScale || scaleZ < minScale) {
            // Don't apply - keep current transform
            return;
        }
        
        // Decompose the LOCAL matrix back to transform
        glm::vec3 scale, translation, skew;
        glm::vec4 perspective;
        glm::quat rotation;
        glm::decompose(localMatrix, scale, rotation, translation, skew, perspective);

        // Additional safety clamp
        scale.x = std::max(scale.x, minScale);
        scale.y = std::max(scale.y, minScale);
        scale.z = std::max(scale.z, minScale);

        pTransform->position[0] = translation.x;
        pTransform->position[1] = translation.y;
        pTransform->position[2] = translation.z;

        pTransform->rotation[0] = rotation.x;
        pTransform->rotation[1] = rotation.y;
        pTransform->rotation[2] = rotation.z;
        pTransform->rotation[3] = rotation.w;

        pTransform->scale[0] = scale.x;
        pTransform->scale[1] = scale.y;
        pTransform->scale[2] = scale.z;

        pTransform->bDirty = true;
    }

    m_bGizmoUsing = ImGuizmo::IsUsing();

    // Cache transform when gizmo starts being used (for undo)
    if (m_bGizmoUsing && !wasUsing) {
        m_cachedPosition[0] = pTransform->position[0];
        m_cachedPosition[1] = pTransform->position[1];
        m_cachedPosition[2] = pTransform->position[2];
        m_cachedRotation[0] = pTransform->rotation[0];
        m_cachedRotation[1] = pTransform->rotation[1];
        m_cachedRotation[2] = pTransform->rotation[2];
        m_cachedRotation[3] = pTransform->rotation[3];
        m_cachedScale[0] = pTransform->scale[0];
        m_cachedScale[1] = pTransform->scale[1];
        m_cachedScale[2] = pTransform->scale[2];
    }
}

void EditorLayer::SetSelectedObject(uint32_t gameObjectId) {
    m_selectedObjectId = gameObjectId;
}

void EditorLayer::SelectAtScreenPos(SceneNew* pScene, Camera* pCamera, float screenX, float screenY, uint32_t viewportW, uint32_t viewportH) {
    if (!pScene || !pCamera || viewportW == 0 || viewportH == 0) return;

    // Convert screen coords to NDC
    float ndcX = (2.0f * screenX / static_cast<float>(viewportW)) - 1.0f;
    float ndcY = 1.0f - (2.0f * screenY / static_cast<float>(viewportH)); // Flip Y

    // Create ray from camera
    glm::mat4 invProj = glm::inverse(pCamera->GetProjectionMatrix());
    glm::mat4 invView = glm::inverse(pCamera->GetViewMatrix());

    glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 rayEye = invProj * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
    glm::vec3 rayWorld = glm::normalize(glm::vec3(invView * rayEye));
    glm::vec3 rayOrigin = pCamera->GetPosition();

    // Simple sphere intersection test for all objects
    float closestT = std::numeric_limits<float>::max();
    uint32_t closestId = UINT32_MAX;

    const auto& gameObjects = pScene->GetGameObjects();
    const auto& transforms = pScene->GetTransforms();

    for (const auto& go : gameObjects) {
        if (!go.bActive || go.transformIndex >= transforms.size()) continue;

        const Transform& t = transforms[go.transformIndex];
        glm::vec3 objPos(t.position[0], t.position[1], t.position[2]);
        
        // Simple sphere of radius 1 (scaled by average scale)
        float avgScale = (t.scale[0] + t.scale[1] + t.scale[2]) / 3.0f;
        float radius = avgScale * 0.5f; // Half unit sphere

        // Ray-sphere intersection
        glm::vec3 oc = rayOrigin - objPos;
        float a = glm::dot(rayWorld, rayWorld);
        float b = 2.0f * glm::dot(oc, rayWorld);
        float c = glm::dot(oc, oc) - radius * radius;
        float discriminant = b * b - 4.0f * a * c;

        if (discriminant >= 0) {
            float tHit = (-b - std::sqrt(discriminant)) / (2.0f * a);
            if (tHit > 0 && tHit < closestT) {
                closestT = tHit;
                closestId = go.id;
            }
        }
    }

    SetSelectedObject(closestId);
}

void EditorLayer::SaveCurrentLevel(SceneNew* pScene) {
    if (!pScene || m_currentLevelPath.empty()) {
        VulkanUtils::LogWarn("Cannot save: no scene or level path not set");
        return;
    }

    try {
        nlohmann::json levelJson;
        levelJson["instances"] = nlohmann::json::array();

        const auto& gameObjects = pScene->GetGameObjects();
        const auto& renderers = pScene->GetRenderers();

        for (size_t goIdx = 0; goIdx < gameObjects.size(); ++goIdx) {
            const auto& go = gameObjects[goIdx];
            const Transform* pTransform = pScene->GetTransform(go.id);
            if (!pTransform) continue;

            // Skip light-only objects (handled separately)
            if (go.HasLight() && !go.HasRenderer()) continue;

            nlohmann::json instance;
            instance["name"] = go.name;

            // Find source path from renderer component if available
            if (go.HasRenderer() && go.rendererIndex < renderers.size()) {
                // Note: RendererComponent doesn't store sourcePath currently.
                // We preserve the name which can be used to identify the object.
                instance["source"] = "mesh:" + go.name;
            } else {
                instance["source"] = "unknown";
            }

            instance["transform"] = {
                {"position", {pTransform->position[0], pTransform->position[1], pTransform->position[2]}},
                {"scale", {pTransform->scale[0], pTransform->scale[1], pTransform->scale[2]}}
            };

            // Convert quaternion to Euler angles for JSON
            glm::quat rot(pTransform->rotation[3], pTransform->rotation[0], pTransform->rotation[1], pTransform->rotation[2]);
            glm::vec3 euler = glm::degrees(glm::eulerAngles(rot));
            instance["transform"]["rotation"] = {euler.x, euler.y, euler.z};

            levelJson["instances"].push_back(instance);
        }

        // Also save lights
        const auto& lights = pScene->GetLights();
        for (size_t lightIdx = 0; lightIdx < lights.size(); ++lightIdx) {
            const auto& light = lights[lightIdx];
            
            // Find the game object that owns this light
            const GameObject* pGo = nullptr;
            for (const auto& obj : gameObjects) {
                if (obj.lightIndex == static_cast<uint32_t>(lightIdx)) {
                    pGo = &obj;
                    break;
                }
            }
            if (!pGo) continue;

            const Transform* pTransform = pScene->GetTransform(pGo->id);
            if (!pTransform) continue;

            nlohmann::json instance;
            instance["source"] = "light";
            instance["name"] = pGo->name;
            instance["transform"] = {
                {"position", {pTransform->position[0], pTransform->position[1], pTransform->position[2]}},
                {"scale", {pTransform->scale[0], pTransform->scale[1], pTransform->scale[2]}}
            };
            glm::quat rot(pTransform->rotation[3], pTransform->rotation[0], pTransform->rotation[1], pTransform->rotation[2]);
            glm::vec3 euler = glm::degrees(glm::eulerAngles(rot));
            instance["transform"]["rotation"] = {euler.x, euler.y, euler.z};

            // Light-specific properties
            instance["light"] = {
                {"type", static_cast<int>(light.type)},
                {"color", {light.color[0], light.color[1], light.color[2]}},
                {"intensity", light.intensity},
                {"range", light.range}
            };

            levelJson["instances"].push_back(instance);
        }

        // Write to file
        std::ofstream outFile(m_currentLevelPath);
        if (outFile.is_open()) {
            outFile << levelJson.dump(2);  // Pretty print with 2-space indent
            outFile.close();
            VulkanUtils::LogInfo("Level saved: {}", m_currentLevelPath);
        } else {
            VulkanUtils::LogErr("Failed to open file for writing: {}", m_currentLevelPath);
        }
    } catch (const std::exception& e) {
        VulkanUtils::LogErr("Error saving level: {}", e.what());
    }
}

void EditorLayer::DrawCamerasPanel(SceneNew* pScene) {
    if (!pScene) return;
    
    ImGui::Begin("Cameras");
    
    // Add camera button
    if (ImGui::Button("+ Add Camera")) {
        // Create a new GameObject with a camera component
        uint32_t newGoId = pScene->CreateGameObject("Camera");
        
        // Set initial position
        Transform* pTransform = pScene->GetTransform(newGoId);
        if (pTransform) {
            TransformSetPosition(*pTransform, 0.f, 2.f, 5.f);
            // Face forward (-Z)
            TransformSetRotation(*pTransform, 0.f, 0.f, 0.f, 1.f);
        }
        
        // Add camera component with default settings
        CameraComponent cam;
        cam.projection = ProjectionType::Perspective;
        cam.fov = 1.0472f;  // ~60 degrees
        cam.nearClip = 0.1f;
        cam.farClip = 1000.0f;
        cam.bIsMain = false;
        pScene->AddCamera(newGoId, cam);
    }
    
    ImGui::Separator();
    
    // List all cameras in the scene
    const auto& gameObjects = pScene->GetGameObjects();
    auto& cameras = pScene->GetCameras();
    
    for (const auto& go : gameObjects) {
        if (!go.HasCamera()) continue;
        
        ImGui::PushID(static_cast<int>(go.id));
        
        // Build camera label
        std::string label = go.name.empty() ? ("Camera " + std::to_string(go.id)) : go.name;
        
        bool opened = ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        
        // Context menu for delete
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete Camera")) {
                pScene->DestroyGameObject(go.id);
                ImGui::EndPopup();
                ImGui::PopID();
                break;  // Exit loop since vector modified
            }
            if (ImGui::MenuItem("Select in Hierarchy")) {
                m_selectedObjectId = go.id;
            }
            ImGui::EndPopup();
        }
        
        if (opened && go.cameraIndex < cameras.size()) {
            CameraComponent& cam = cameras[go.cameraIndex];
            Transform* pTransform = pScene->GetTransform(go.id);
            
            // Camera name (editable)
            char nameBuf[64] = {};
            const std::string& goName = go.name;
            size_t copyLen = goName.size() < sizeof(nameBuf) - 1 ? goName.size() : sizeof(nameBuf) - 1;
            for (size_t ci = 0; ci < copyLen; ++ci) {
                nameBuf[ci] = goName[ci];
            }
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                // Need mutable access to change name
                GameObject* pGoMut = pScene->FindGameObject(go.id);
                if (pGoMut) pGoMut->name = nameBuf;
            }
            
            // Main camera checkbox
            if (ImGui::Checkbox("Is Main Camera", &cam.bIsMain)) {
                // If setting this as main, unset others
                if (cam.bIsMain) {
                    for (auto& otherCam : cameras) {
                        if (&otherCam != &cam) otherCam.bIsMain = false;
                    }
                }
            }
            
            // Transform: Position
            if (pTransform) {
                float pos[3] = {pTransform->position[0], pTransform->position[1], pTransform->position[2]};
                if (ImGui::DragFloat3("Position", pos, 0.1f)) {
                    TransformSetPosition(*pTransform, pos[0], pos[1], pos[2]);
                }
                
                // Transform: Rotation (show as Euler degrees for ease)
                // Convert quaternion to Euler (approximate)
                float yaw = std::atan2(2.0f * (pTransform->rotation[3] * pTransform->rotation[1] + 
                                              pTransform->rotation[0] * pTransform->rotation[2]),
                                       1.0f - 2.0f * (pTransform->rotation[1] * pTransform->rotation[1] + 
                                                      pTransform->rotation[2] * pTransform->rotation[2]));
                float pitch = std::asin(std::clamp(2.0f * (pTransform->rotation[3] * pTransform->rotation[0] - 
                                                          pTransform->rotation[2] * pTransform->rotation[1]), -1.0f, 1.0f));
                
                float yawDeg = yaw * 180.0f / 3.14159265f;
                float pitchDeg = pitch * 180.0f / 3.14159265f;
                
                bool rotChanged = false;
                rotChanged |= ImGui::DragFloat("Yaw (Y)", &yawDeg, 1.0f, -180.0f, 180.0f);
                rotChanged |= ImGui::DragFloat("Pitch (X)", &pitchDeg, 1.0f, -89.0f, 89.0f);
                
                if (rotChanged) {
                    // Convert back to quaternion
                    float yawRad = yawDeg * 3.14159265f / 180.0f;
                    float pitchRad = pitchDeg * 3.14159265f / 180.0f;
                    
                    float cy = std::cos(yawRad * 0.5f);
                    float sy = std::sin(yawRad * 0.5f);
                    float cp = std::cos(pitchRad * 0.5f);
                    float sp = std::sin(pitchRad * 0.5f);
                    
                    // Order: Y (yaw) then X (pitch)
                    float qx = cy * sp;
                    float qy = sy * cp;
                    float qz = -sy * sp;
                    float qw = cy * cp;
                    
                    TransformSetRotation(*pTransform, qx, qy, qz, qw);
                }
            }
            
            ImGui::Separator();
            
            // Projection type
            const char* projTypes[] = {"Perspective", "Orthographic"};
            int projIdx = static_cast<int>(cam.projection);
            if (ImGui::Combo("Projection", &projIdx, projTypes, IM_ARRAYSIZE(projTypes))) {
                cam.projection = static_cast<ProjectionType>(projIdx);
            }
            
            // Projection-specific settings
            if (cam.projection == ProjectionType::Perspective) {
                float fovDeg = cam.fov * 180.0f / 3.14159265f;
                if (ImGui::DragFloat("FOV (degrees)", &fovDeg, 1.0f, 10.0f, 150.0f)) {
                    cam.fov = fovDeg * 3.14159265f / 180.0f;
                }
            } else {
                ImGui::DragFloat("Ortho Size", &cam.orthoSize, 0.1f, 0.1f, 100.0f);
            }
            
            // Clip planes
            ImGui::DragFloat("Near Clip", &cam.nearClip, 0.01f, 0.001f, cam.farClip - 0.001f);
            ImGui::DragFloat("Far Clip", &cam.farClip, 1.0f, cam.nearClip + 0.001f, 100000.0f);
            
            // Aspect ratio override
            ImGui::DragFloat("Aspect Ratio Override", &cam.aspectRatio, 0.01f, 0.0f, 4.0f, "%.2f (0 = auto)");
            
            // Clear settings
            const char* clearFlags[] = {"Skybox", "Solid Color", "Depth Only", "Nothing"};
            int clearIdx = static_cast<int>(cam.clearFlags);
            if (ImGui::Combo("Clear Flags", &clearIdx, clearFlags, IM_ARRAYSIZE(clearFlags))) {
                cam.clearFlags = static_cast<CameraClearFlags>(clearIdx);
            }
            
            if (cam.clearFlags == CameraClearFlags::SolidColor) {
                ImGui::ColorEdit4("Clear Color", cam.clearColor);
            }
            
            // Render depth (priority)
            ImGui::DragInt("Depth (priority)", &cam.depth, 1, -100, 100);
            
            // Culling mask
            if (ImGui::TreeNode("Culling Mask")) {
                // Show as bit flags (simplified)
                for (int layer = 0; layer < 8; ++layer) {
                    bool layerEnabled = (cam.cullingMask & (1u << layer)) != 0;
                    char layerName[16];
                    snprintf(layerName, sizeof(layerName), "Layer %d", layer);
                    if (ImGui::Checkbox(layerName, &layerEnabled)) {
                        if (layerEnabled)
                            cam.cullingMask |= (1u << layer);
                        else
                            cam.cullingMask &= ~(1u << layer);
                    }
                }
                ImGui::TreePop();
            }
        }
        
        ImGui::PopID();
        ImGui::Separator();
    }
    
    ImGui::End();
}

void EditorLayer::DrawViewportsPanel(ViewportManager* pViewportManager, SceneNew* pScene) {
    if (!pViewportManager) return;
    
    ImGui::Begin("Viewports");
    
    // Add viewport button
    if (ImGui::Button("+ Add Viewport")) {
        ViewportConfig newConfig;
        newConfig.name = "PIP Viewport " + std::to_string(pViewportManager->GetNextId());
        newConfig.bIsMainViewport = false;
        newConfig.pipPosition = {0.7f, 0.02f};
        newConfig.pipSize = {320.0f, 180.0f};
        newConfig.renderMode = ViewportRenderMode::Solid;
        pViewportManager->AddViewport(newConfig);
    }
    
    ImGui::Separator();
    
    // List all viewports
    auto& viewports = pViewportManager->GetViewports();
    for (size_t i = 0; i < viewports.size(); ++i) {
        Viewport& vp = viewports[i];
        ViewportConfig& config = vp.config;
        
        ImGui::PushID(static_cast<int>(i));
        
        // Collapsing header for each viewport
        bool opened = ImGui::CollapsingHeader(config.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        
        // Context menu for delete (except main viewport)
        if (!config.bIsMainViewport && ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete Viewport")) {
                pViewportManager->RemoveViewport(config.id);
                ImGui::EndPopup();
                ImGui::PopID();
                break;  // Exit loop since vector modified
            }
            ImGui::EndPopup();
        }
        
        if (opened) {
            // Viewport name
            char nameBuf[64] = {};
            size_t copyLen = config.name.size() < sizeof(nameBuf) - 1 ? config.name.size() : sizeof(nameBuf) - 1;
            for (size_t ci = 0; ci < copyLen; ++ci) {
                nameBuf[ci] = config.name[ci];
            }
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                config.name = nameBuf;
            }
            
            // Visibility toggle
            ImGui::Checkbox("Visible", &config.bVisible);
            
            // Render mode dropdown
            const char* renderModes[] = {"Solid", "Wireframe", "Unlit", "Normals", "Depth", "UV"};
            int currentMode = static_cast<int>(config.renderMode);
            
            if (ImGui::Combo("Render Mode", &currentMode, renderModes, IM_ARRAYSIZE(renderModes))) {
                config.renderMode = static_cast<ViewportRenderMode>(currentMode);
            }
            
            // Camera selector - populate from scene cameras
            {
                // Build list of camera options: Main Camera + any cameras in scene
                std::vector<std::pair<uint32_t, std::string>> cameraOptions;
                cameraOptions.push_back({UINT32_MAX, "Main Camera"});
                
                if (pScene) {
                    const auto& gameObjects = pScene->GetGameObjects();
                    for (const auto& go : gameObjects) {
                        if (go.bActive && go.HasCamera()) {
                            std::string label = go.name.empty() ? ("Camera " + std::to_string(go.id)) : go.name;
                            cameraOptions.push_back({go.id, label});
                        }
                    }
                }
                
                // Find current selection index
                int currentCamera = 0;
                for (size_t camIdx = 0; camIdx < cameraOptions.size(); ++camIdx) {
                    if (cameraOptions[camIdx].first == config.cameraGameObjectId) {
                        currentCamera = static_cast<int>(camIdx);
                        break;
                    }
                }
                
                // Build combo string
                std::string comboItems;
                for (const auto& opt : cameraOptions) {
                    comboItems += opt.second;
                    comboItems += '\0';
                }
                comboItems += '\0';
                
                if (ImGui::Combo("Camera", &currentCamera, comboItems.c_str())) {
                    if (currentCamera >= 0 && currentCamera < static_cast<int>(cameraOptions.size())) {
                        config.cameraGameObjectId = cameraOptions[currentCamera].first;
                    }
                }
            }
            
            // Only show PIP settings for non-main viewports
            if (!config.bIsMainViewport) {
                ImGui::Text("Position");
                ImGui::Indent();
                ImGui::DragFloat("PIP X", &config.pipPosition.x, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("PIP Y", &config.pipPosition.y, 0.01f, 0.0f, 1.0f);
                ImGui::Unindent();
                
                ImGui::Text("Size");
                ImGui::Indent();
                float pipW = config.pipSize.x;
                float pipH = config.pipSize.y;
                if (ImGui::DragFloat("Width", &pipW, 1.0f, 64.0f, 1920.0f)) {
                    pViewportManager->ResizeViewport(config.id, static_cast<uint32_t>(pipW), static_cast<uint32_t>(config.pipSize.y));
                }
                if (ImGui::DragFloat("Height", &pipH, 1.0f, 64.0f, 1080.0f)) {
                    pViewportManager->ResizeViewport(config.id, static_cast<uint32_t>(config.pipSize.x), static_cast<uint32_t>(pipH));
                }
                ImGui::Unindent();
                
                // Detach button (future: pop out to separate window)
                ImGui::Checkbox("Detached", &config.bDetached);
            }
            
            // Clear color
            float clearCol[4] = {config.clearColor.r, config.clearColor.g, config.clearColor.b, config.clearColor.a};
            if (ImGui::ColorEdit4("Clear Color", clearCol)) {
                config.clearColor = {clearCol[0], clearCol[1], clearCol[2], clearCol[3]};
            }
            
            // Gizmos/Grid toggles
            ImGui::Checkbox("Show Gizmos", &config.bShowGizmos);
            ImGui::SameLine();
            ImGui::Checkbox("Show Grid", &config.bShowGrid);
            
            // Post-processing flags
            if (ImGui::TreeNode("Post-Processing")) {
                bool hasToneMapping = HasFlag(config.postProcess, ViewportPostProcess::ToneMapping);
                if (ImGui::Checkbox("Tone Mapping", &hasToneMapping)) {
                    if (hasToneMapping)
                        config.postProcess = config.postProcess | ViewportPostProcess::ToneMapping;
                    else
                        config.postProcess = static_cast<ViewportPostProcess>(static_cast<uint32_t>(config.postProcess) & ~static_cast<uint32_t>(ViewportPostProcess::ToneMapping));
                }
                
                bool hasBloom = HasFlag(config.postProcess, ViewportPostProcess::Bloom);
                if (ImGui::Checkbox("Bloom", &hasBloom)) {
                    if (hasBloom)
                        config.postProcess = config.postProcess | ViewportPostProcess::Bloom;
                    else
                        config.postProcess = static_cast<ViewportPostProcess>(static_cast<uint32_t>(config.postProcess) & ~static_cast<uint32_t>(ViewportPostProcess::Bloom));
                }
                
                bool hasFXAA = HasFlag(config.postProcess, ViewportPostProcess::FXAA);
                if (ImGui::Checkbox("FXAA", &hasFXAA)) {
                    if (hasFXAA)
                        config.postProcess = config.postProcess | ViewportPostProcess::FXAA;
                    else
                        config.postProcess = static_cast<ViewportPostProcess>(static_cast<uint32_t>(config.postProcess) & ~static_cast<uint32_t>(ViewportPostProcess::FXAA));
                }
                
                ImGui::TreePop();
            }
            
            // Preview for non-main viewports (show rendered image)
            if (!config.bIsMainViewport && vp.renderTarget.imguiTextureId != VK_NULL_HANDLE) {
                ImGui::Separator();
                ImGui::Text("Preview:");
                float previewW = ImGui::GetContentRegionAvail().x;
                float aspectRatio = config.pipSize.y / config.pipSize.x;
                float previewH = previewW * aspectRatio;
                ImGui::Image((ImTextureID)(uintptr_t)vp.renderTarget.imguiTextureId, ImVec2(previewW, previewH));
            }
        }
        
        ImGui::PopID();
        ImGui::Separator();
    }
    
    ImGui::End();
    
    // Draw additional viewports as dockable panels
    for (size_t i = 0; i < viewports.size(); ++i) {
        Viewport& vp = viewports[i];
        ViewportConfig& config = vp.config;
        
        // Skip main viewport (handled by DrawViewportPanel) and invisible viewports
        if (config.bIsMainViewport || !config.bVisible) {
            continue;
        }
        
        if (vp.renderTarget.imguiTextureId == VK_NULL_HANDLE) {
            continue;
        }
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        
        ImGuiWindowFlags vpFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        
        std::string vpTitle = config.name + "###VP" + std::to_string(config.id);
        if (ImGui::Begin(vpTitle.c_str(), &config.bVisible, vpFlags)) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            
            // Resize render target if needed
            uint32_t newW = static_cast<uint32_t>(avail.x);
            uint32_t newH = static_cast<uint32_t>(avail.y);
            if (newW > 0 && newH > 0 && 
                (vp.renderTarget.width != newW || vp.renderTarget.height != newH)) {
                pViewportManager->ResizeViewport(config.id, newW, newH);
            }
            
            ImGui::Image((ImTextureID)(uintptr_t)vp.renderTarget.imguiTextureId, avail);
            
            // Track hover/focus for interaction
            vp.bHovered = ImGui::IsWindowHovered();
            vp.bFocused = ImGui::IsWindowFocused();
        }
        ImGui::End();
        
        ImGui::PopStyleVar();
    }
}

#endif // EDITOR_BUILD
