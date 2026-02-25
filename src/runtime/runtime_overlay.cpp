/*
 * RuntimeOverlay - Minimal ImGui overlay implementation.
 */
#include "runtime_overlay.h"
#include "camera/camera.h"
#include "config/vulkan_config.h"

#include <imgui.h>
#include <algorithm>

void RuntimeOverlay::Init(
    SDL_Window* pWindow,
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    uint32_t graphicsQueueFamily,
    VkQueue graphicsQueue,
    VkRenderPass renderPass,
    uint32_t imageCount
) {
    // Initialize base ImGui - no docking or viewports for runtime overlay
    InitImGui(pWindow, instance, physicalDevice, device,
              graphicsQueueFamily, graphicsQueue, renderPass, imageCount,
              false,  // enableDocking
              false); // enableViewports
}

void RuntimeOverlay::Update(float deltaTime) {
    m_deltaTime = deltaTime;
    
    // Calculate FPS
    if (deltaTime > 0.0f) {
        const float instantFps = 1.0f / deltaTime;
        m_fps = kSmoothingFactor * m_fps + (1.0f - kSmoothingFactor) * instantFps;
    }
    
    // Update frame time stats
    const float frameMs = deltaTime * 1000.0f;
    m_avgFrameTime = kSmoothingFactor * m_avgFrameTime + (1.0f - kSmoothingFactor) * frameMs;
    m_minFrameTime = std::min(m_minFrameTime, frameMs);
    m_maxFrameTime = std::max(m_maxFrameTime, frameMs);
    
    // Update FPS history for graph
    m_fpsHistory[m_fpsHistoryIndex] = m_fps;
    m_fpsHistoryIndex = (m_fpsHistoryIndex + 1) % kFpsHistorySize;
}

void RuntimeOverlay::Draw(const Camera* pCamera, const VulkanConfig* pConfig) {
    if (!IsInitialized() || !IsEnabled() || !m_bVisible) return;
    
    m_pCurrentCamera = pCamera;
    m_pCurrentConfig = pConfig;
    
    BeginFrame();
    DrawContent();
    EndFrame();
}

void RuntimeOverlay::DrawContent() {
    DrawStatsWindow(m_pCurrentCamera, m_pCurrentConfig);
}

void RuntimeOverlay::DrawStatsWindow(const Camera* pCamera, const VulkanConfig* pConfig) {
    ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove;
    
    // Position based on corner setting
    const float padding = 10.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 workPos = viewport->WorkPos;
    ImVec2 workSize = viewport->WorkSize;
    
    ImVec2 windowPos;
    ImVec2 windowPivot;
    
    switch (m_corner) {
        case 0:  // Top-left
            windowPos = ImVec2(workPos.x + padding, workPos.y + padding);
            windowPivot = ImVec2(0.0f, 0.0f);
            break;
        case 1:  // Top-right
            windowPos = ImVec2(workPos.x + workSize.x - padding, workPos.y + padding);
            windowPivot = ImVec2(1.0f, 0.0f);
            break;
        case 2:  // Bottom-left
            windowPos = ImVec2(workPos.x + padding, workPos.y + workSize.y - padding);
            windowPivot = ImVec2(0.0f, 1.0f);
            break;
        case 3:  // Bottom-right
        default:
            windowPos = ImVec2(workPos.x + workSize.x - padding, workPos.y + workSize.y - padding);
            windowPivot = ImVec2(1.0f, 1.0f);
            break;
    }
    
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPivot);
    ImGui::SetNextWindowBgAlpha(0.6f);
    
    if (ImGui::Begin("Stats##RuntimeOverlay", nullptr, windowFlags)) {
        // FPS and frame time
        ImGui::Text("FPS: %.1f", m_fps);
        ImGui::Text("Frame: %.2f ms", m_avgFrameTime);
        ImGui::Text("Min/Max: %.2f / %.2f ms", m_minFrameTime, m_maxFrameTime);
        
        // FPS graph
        ImGui::Separator();
        ImGui::PlotLines("##FPSGraph", m_fpsHistory, kFpsHistorySize, m_fpsHistoryIndex,
                         nullptr, 0.0f, 120.0f, ImVec2(150, 40));
        
        // Render statistics
        if (m_renderStats.objectsTotal > 0 || m_renderStats.drawCalls > 0) {
            ImGui::Separator();
            ImGui::Text("Draw Calls: %u", m_renderStats.drawCalls);
            ImGui::Text("Objects: %u / %u", m_renderStats.objectsVisible, m_renderStats.objectsTotal);
            ImGui::Text("Triangles: %u", m_renderStats.triangles);
            ImGui::Text("Vertices: %u", m_renderStats.vertices);
            if (m_renderStats.objectsTotal > 0) {
                const float cullPct = (1.0f - m_renderStats.cullingRatio) * 100.0f;
                ImGui::Text("Culled: %.1f%%", cullPct);
            }
        }
        
        // Instance tier statistics with draw calls
        const uint32_t totalInstances = m_renderStats.instancesStatic + 
                                        m_renderStats.instancesSemiStatic + 
                                        m_renderStats.instancesDynamic + 
                                        m_renderStats.instancesProcedural;
        if (totalInstances > 0) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Instancing Tiers (obj / draws)");
            ImGui::Text("Static:      %3u / %u", m_renderStats.instancesStatic, m_renderStats.drawCallsStatic);
            ImGui::Text("Semi-Static: %3u / %u", m_renderStats.instancesSemiStatic, m_renderStats.drawCallsSemiStatic);
            ImGui::Text("Dynamic:     %3u / %u", m_renderStats.instancesDynamic, m_renderStats.drawCallsDynamic);
            ImGui::Text("Procedural:  %3u / %u", m_renderStats.instancesProcedural, m_renderStats.drawCallsProcedural);
            
            // Instancing efficiency
            const uint32_t totalDraws = m_renderStats.drawCallsStatic + m_renderStats.drawCallsSemiStatic +
                                        m_renderStats.drawCallsDynamic + m_renderStats.drawCallsProcedural;
            if (totalDraws > 0) {
                const float efficiency = static_cast<float>(totalInstances) / static_cast<float>(totalDraws);
                ImGui::Text("Efficiency:  %.1fx", efficiency);
            }
        }
        
        // Camera info (if available)
        if (pCamera) {
            ImGui::Separator();
            const auto& pos = pCamera->GetPosition();
            ImGui::Text("Pos: %.1f, %.1f, %.1f", pos.x, pos.y, pos.z);
        }
        
        // Resolution (if config available)
        if (pConfig) {
            ImGui::Separator();
            ImGui::Text("%dx%d", pConfig->lWidth, pConfig->lHeight);
        }
        
        // Controls hint
        ImGui::Separator();
        ImGui::TextDisabled("F3: Toggle overlay");
    }
    ImGui::End();
}
