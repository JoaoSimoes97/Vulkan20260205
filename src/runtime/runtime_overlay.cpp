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
            
            // SSBO uploads per tier this frame
            const uint32_t totalUploads = m_renderStats.uploadsStatic + m_renderStats.uploadsSemiStatic +
                                          m_renderStats.uploadsDynamic + m_renderStats.uploadsProcedural;
            if (totalUploads > 0 || totalInstances > 0) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.7f, 1.0f), "SSBO Uploads (this frame)");
                ImGui::Text("Static:      %3u", m_renderStats.uploadsStatic);
                ImGui::Text("Semi-Static: %3u", m_renderStats.uploadsSemiStatic);
                ImGui::Text("Dynamic:     %3u", m_renderStats.uploadsDynamic);
                ImGui::Text("Procedural:  %3u", m_renderStats.uploadsProcedural);
                ImGui::Text("Total:       %3u / %u", totalUploads, totalInstances);
            }
        }
        
        // GPU culling statistics
        if (m_renderStats.gpuCullerActive) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "GPU Culling");
            ImGui::Text("GPU Visible: %u / %u", m_renderStats.gpuCulledVisible, m_renderStats.gpuCulledTotal);
            if (m_renderStats.gpuCulledTotal > 0) {
                const float gpuCullPct = (1.0f - static_cast<float>(m_renderStats.gpuCulledVisible) / 
                                         static_cast<float>(m_renderStats.gpuCulledTotal)) * 100.0f;
                ImGui::Text("GPU Culled: %.1f%%", gpuCullPct);
            }
            if (m_renderStats.gpuCpuMismatch) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "CPU/GPU MISMATCH!");
            } else {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "CPU/GPU Match OK");
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
    
    // Draw level selector as separate window
    DrawLevelSelector();
}

void RuntimeOverlay::DrawLevelSelector() {
    if (!m_pLevelSelector) return;
    
    const auto& levels = m_pLevelSelector->GetLevels();
    if (levels.empty()) return;
    
    ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings;
    
    // Position at bottom-right
    const float padding = 10.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 workPos = viewport->WorkPos;
    ImVec2 workSize = viewport->WorkSize;
    ImVec2 windowPos(workPos.x + workSize.x - padding, workPos.y + workSize.y - padding);
    
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_FirstUseEver, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.8f);
    
    if (ImGui::Begin("Level Selector", nullptr, windowFlags)) {
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.5f, 1.0f), "Scene Selection");
        ImGui::Separator();
        
        // Current level
        const std::string& currentPath = m_pLevelSelector->GetCurrentLevelPath();
        if (!currentPath.empty()) {
            ImGui::TextDisabled("Current: %s", currentPath.c_str());
        }
        
        // Level combo box
        int selectedIdx = m_pLevelSelector->GetSelectedIndex();
        const char* previewName = (selectedIdx >= 0 && selectedIdx < static_cast<int>(levels.size())) 
            ? levels[selectedIdx].name.c_str() 
            : "Select a level...";
        
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::BeginCombo("##LevelCombo", previewName)) {
            for (int i = 0; i < static_cast<int>(levels.size()); ++i) {
                const LevelInfo& level = levels[i];
                
                // Separator items are not selectable
                if (level.isSpecial && level.specialId == 0) {
                    ImGui::TextDisabled("%s", level.name.c_str());
                    continue;
                }
                
                bool isSelected = (selectedIdx == i);
                
                // Color stress tests differently
                if (level.isSpecial) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.3f, 1.0f));
                }
                
                if (ImGui::Selectable(level.name.c_str(), isSelected)) {
                    m_pLevelSelector->SetSelectedIndex(i);
                }
                
                if (level.isSpecial) {
                    ImGui::PopStyleColor();
                }
                
                // Tooltip with description
                if (!level.description.empty() && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", level.description.c_str());
                }
                
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        
        // Load button
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            m_pLevelSelector->RequestLoad();
        }
        
        // Show custom stress test sliders when selected (specialId == 5)
        const LevelInfo* selected = m_pLevelSelector->GetSelectedLevel();
        if (selected && selected->isSpecial && selected->specialId == 5) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.7f, 1.0f), "Custom Parameters");
            
            StressTestParams& params = m_pLevelSelector->GetCustomParams();
            
            // Slider width
            ImGui::PushItemWidth(150.0f);
            
            // Static tier
            int staticCount = static_cast<int>(params.staticCount);
            if (ImGui::SliderInt("Static", &staticCount, 0, 100000, "%d")) {
                params.staticCount = static_cast<uint32_t>(staticCount);
            }
            
            // Semi-static tier
            int semiCount = static_cast<int>(params.semiStaticCount);
            if (ImGui::SliderInt("Semi-Static", &semiCount, 0, 10000, "%d")) {
                params.semiStaticCount = static_cast<uint32_t>(semiCount);
            }
            
            // Dynamic tier
            int dynCount = static_cast<int>(params.dynamicCount);
            if (ImGui::SliderInt("Dynamic", &dynCount, 0, 10000, "%d")) {
                params.dynamicCount = static_cast<uint32_t>(dynCount);
            }
            
            // Procedural tier
            int procCount = static_cast<int>(params.proceduralCount);
            if (ImGui::SliderInt("Procedural", &procCount, 0, 20000, "%d")) {
                params.proceduralCount = static_cast<uint32_t>(procCount);
            }
            
            ImGui::PopItemWidth();
            
            // Show total
            uint32_t total = GetStressTestObjectCount(params);
            ImGui::TextDisabled("Total: %u objects", total);
        } else if (selected && !selected->description.empty()) {
            // Show selected level description for non-custom levels
            ImGui::TextWrapped("%s", selected->description.c_str());
        }
    }
    ImGui::End();
}
