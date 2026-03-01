/*
 * MainMenu - Implementation
 */
#include "main_menu.h"
#include "config/vulkan_config.h"

#include <imgui.h>
#include <algorithm>

void MainMenu::Init(
    SDL_Window* pWindow,
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    uint32_t graphicsQueueFamily,
    VkQueue graphicsQueue,
    VkRenderPass renderPass,
    uint32_t imageCount
) {
    // Initialize base ImGui - no docking, no viewports for main menu
    InitImGui(pWindow, instance, physicalDevice, device,
              graphicsQueueFamily, graphicsQueue, renderPass, imageCount,
              false,  // enableDocking
              false); // enableViewports
}

void MainMenu::Draw(VulkanConfig* pConfig) {
    if (!IsInitialized() || !IsEnabled() || !m_bVisible) return;
    
    m_pCurrentConfig = pConfig;
    
    BeginFrame();
    DrawContent();
    EndFrame();
}

void MainMenu::DrawContent() {
    switch (m_state) {
        case MainMenuState::Main:
            DrawMainPage();
            break;
        case MainMenuState::LevelSelect:
            DrawLevelSelectPage();
            break;
        case MainMenuState::Settings:
            DrawSettingsPage(m_pCurrentConfig);
            break;
    }
}

void MainMenu::DrawMainPage() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    
    // Full-screen dark overlay
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowBgAlpha(0.85f);
    
    ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    
    if (ImGui::Begin("##MainMenuBackground", nullptr, windowFlags)) {
        // Title (centered via SetCursorPosX/Y below)
        const char* title = "VULKAN ENGINE";
        ImGui::PushFont(nullptr);  // Use default font
        ImVec2 titleSize = ImGui::CalcTextSize(title);
        ImGui::SetCursorPosX((viewport->WorkSize.x - titleSize.x) * 0.5f);
        ImGui::SetCursorPosY(viewport->WorkSize.y * 0.2f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.5f, 1.0f));
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        ImGui::PopFont();
        
        // Subtitle
        const char* subtitle = m_bLevelLoaded ? "Game paused" : "Select an option to continue";
        ImVec2 subtitleSize = ImGui::CalcTextSize(subtitle);
        ImGui::SetCursorPosX((viewport->WorkSize.x - subtitleSize.x) * 0.5f);
        ImGui::TextDisabled("%s", subtitle);
        
        // Menu buttons - centered
        ImGui::Spacing();
        ImGui::Spacing();
        
        const float buttonWidth = 250.0f;
        const float buttonHeight = 50.0f;
        const float buttonX = (viewport->WorkSize.x - buttonWidth) * 0.5f;
        const float startY = viewport->WorkSize.y * 0.4f;
        
        ImGui::SetCursorPosY(startY);
        
        // Resume button (only when level is loaded)
        if (m_bLevelLoaded) {
            ImGui::SetCursorPosX(buttonX);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.4f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.2f, 1.0f));
            if (ImGui::Button("Resume", ImVec2(buttonWidth, buttonHeight))) {
                m_bVisible = false;  // Hide menu to resume gameplay
            }
            ImGui::PopStyleColor(3);
            
            ImGui::Spacing();
            ImGui::Spacing();
        }
        
        // Play / Select Level button
        ImGui::SetCursorPosX(buttonX);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.4f, 0.1f, 1.0f));
        const char* playButtonText = m_bLevelLoaded ? "Change Level" : "Play";
        if (ImGui::Button(playButtonText, ImVec2(buttonWidth, buttonHeight))) {
            m_state = MainMenuState::LevelSelect;
        }
        ImGui::PopStyleColor(3);
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Settings button
        ImGui::SetCursorPosX(buttonX);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.7f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.4f, 1.0f));
        if (ImGui::Button("Settings", ImVec2(buttonWidth, buttonHeight))) {
            m_state = MainMenuState::Settings;
        }
        ImGui::PopStyleColor(3);
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Quit button
        ImGui::SetCursorPosX(buttonX);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button("Quit", ImVec2(buttonWidth, buttonHeight))) {
            m_bQuitRequested = true;
            if (m_quitCallback) {
                m_quitCallback();
            }
        }
        ImGui::PopStyleColor(3);
        
        // Version/credits at bottom
        const char* versionText = "João Simões - 2026";
        ImVec2 versionSize = ImGui::CalcTextSize(versionText);
        ImGui::SetCursorPosX((viewport->WorkSize.x - versionSize.x) * 0.5f);
        ImGui::SetCursorPosY(viewport->WorkSize.y - 40.0f);
        ImGui::TextDisabled("%s", versionText);
    }
    ImGui::End();
}

void MainMenu::DrawLevelSelectPage() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    
    // Full-screen dark overlay
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowBgAlpha(0.90f);
    
    ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    
    if (ImGui::Begin("##LevelSelectBackground", nullptr, windowFlags)) {
        // Back button
        ImGui::SetCursorPos(ImVec2(20.0f, 20.0f));
        if (ImGui::Button("<< Back", ImVec2(100, 30))) {
            m_state = MainMenuState::Main;
        }
        
        // Title
        const char* title = "SELECT LEVEL";
        ImVec2 titleSize = ImGui::CalcTextSize(title);
        ImGui::SetCursorPosX((viewport->WorkSize.x - titleSize.x) * 0.5f);
        ImGui::SetCursorPosY(60.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.5f, 1.0f));
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        
        ImGui::Separator();
        
        if (!m_pLevelSelector) {
            ImGui::TextDisabled("No levels available");
        } else {
            const auto& levels = m_pLevelSelector->GetLevels();
            if (levels.empty()) {
                ImGui::TextDisabled("No levels found in 'levels' folder");
            } else {
                // Level list in scrollable region
                const float listStartY = 100.0f;
                const float listHeight = viewport->WorkSize.y - listStartY - 80.0f;
                const float cardWidth = 350.0f;
                
                ImGui::SetCursorPosY(listStartY);
                
                // Center the list
                const float listX = (viewport->WorkSize.x - cardWidth) * 0.5f;
                ImGui::SetCursorPosX(listX);
                
                ImGui::BeginChild("##LevelList", ImVec2(cardWidth + 20.0f, listHeight), true);
                
                int selectedIdx = m_pLevelSelector->GetSelectedIndex();
                
                for (int i = 0; i < static_cast<int>(levels.size()); ++i) {
                    const LevelInfo& level = levels[i];
                    
                    // Skip separator items but show them as dividers
                    if (level.isSpecial && level.specialId == 0) {
                        ImGui::Separator();
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", level.name.c_str());
                        ImGui::Separator();
                        continue;
                    }
                    
                    bool isSelected = (selectedIdx == i);
                    DrawLevelCard(level, i, isSelected);
                    
                    ImGui::Spacing();
                }
                
                ImGui::EndChild();
                
                // Load button at bottom
                const float buttonWidth = 200.0f;
                const float buttonX = (viewport->WorkSize.x - buttonWidth) * 0.5f;
                ImGui::SetCursorPosX(buttonX);
                ImGui::SetCursorPosY(viewport->WorkSize.y - 60.0f);
                
                const LevelInfo* selected = m_pLevelSelector->GetSelectedLevel();
                const bool canLoad = (selected != nullptr && !(selected->isSpecial && selected->specialId == 0));
                
                if (!canLoad) {
                    ImGui::BeginDisabled();
                }
                
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.4f, 0.1f, 1.0f));
                
                if (ImGui::Button("Start Level", ImVec2(buttonWidth, 40))) {
                    m_pLevelSelector->RequestLoad();
                    m_bLevelLoadRequested = true;
                    m_bVisible = false;  // Hide menu when level loads
                }
                
                ImGui::PopStyleColor(3);
                
                if (!canLoad) {
                    ImGui::EndDisabled();
                }
            }
        }
    }
    ImGui::End();
}

void MainMenu::DrawLevelCard(const LevelInfo& level, int index, bool isSelected) {
    const float cardWidth = ImGui::GetContentRegionAvail().x - 10.0f;
    const float cardHeight = 60.0f;
    
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    
    // Card background color
    ImVec4 bgColor = isSelected 
        ? ImVec4(0.3f, 0.4f, 0.6f, 1.0f)
        : (level.isSpecial ? ImVec4(0.3f, 0.25f, 0.2f, 1.0f) : ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
    
    ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(bgColor.x + 0.1f, bgColor.y + 0.1f, bgColor.z + 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(bgColor.x - 0.05f, bgColor.y - 0.05f, bgColor.z - 0.05f, 1.0f));
    
    ImGui::PushID(index);
    if (ImGui::Button("##LevelCard", ImVec2(cardWidth, cardHeight))) {
        m_pLevelSelector->SetSelectedIndex(index);
    }
    
    // Double-click to load immediately
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        m_pLevelSelector->SetSelectedIndex(index);
        m_pLevelSelector->RequestLoad();
        m_bLevelLoadRequested = true;
        m_bVisible = false;
    }
    
    ImGui::PopID();
    ImGui::PopStyleColor(3);
    
    // Draw level info on top of button
    ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 10.0f, cursorPos.y + 8.0f));
    
    // Level name
    if (level.isSpecial) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", level.name.c_str());
    } else {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", level.name.c_str());
    }
    
    // Description
    ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 10.0f, cursorPos.y + 28.0f));
    if (!level.description.empty()) {
        // Truncate long descriptions
        std::string desc = level.description;
        if (desc.length() > 50) {
            desc = desc.substr(0, 47) + "...";
        }
        ImGui::TextDisabled("%s", desc.c_str());
    } else if (!level.isSpecial) {
        ImGui::TextDisabled("%s", level.path.c_str());
    }
    
    // Reset cursor after card
    ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y + cardHeight));
}

void MainMenu::DrawSettingsPage(VulkanConfig* pConfig) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    
    // Full-screen dark overlay
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowBgAlpha(0.90f);
    
    ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    
    if (ImGui::Begin("##SettingsBackground", nullptr, windowFlags)) {
        // Back button
        ImGui::SetCursorPos(ImVec2(20.0f, 20.0f));
        if (ImGui::Button("<< Back", ImVec2(100, 30))) {
            m_state = MainMenuState::Main;
        }
        
        // Title
        const char* title = "SETTINGS";
        ImVec2 titleSize = ImGui::CalcTextSize(title);
        ImGui::SetCursorPosX((viewport->WorkSize.x - titleSize.x) * 0.5f);
        ImGui::SetCursorPosY(60.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.5f, 1.0f));
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        
        ImGui::Separator();
        
        if (!pConfig) {
            ImGui::TextDisabled("Settings not available");
        } else {
            // Settings content - centered
            const float settingsWidth = 400.0f;
            const float settingsX = (viewport->WorkSize.x - settingsWidth) * 0.5f;
            
            ImGui::SetCursorPosY(120.0f);
            ImGui::SetCursorPosX(settingsX);
            
            ImGui::BeginChild("##SettingsContent", ImVec2(settingsWidth, viewport->WorkSize.y - 180.0f), true);
            
            // Graphics section
            ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "Graphics");
            ImGui::Separator();
            
            // Resolution (display only for now)
            ImGui::Text("Resolution: %dx%d", pConfig->lWidth, pConfig->lHeight);
            
            // VSync / Present mode
            const char* presentModes[] = { "Immediate (No VSync)", "FIFO (VSync)", "Mailbox", "FIFO Relaxed" };
            int currentMode = 0;
            switch (pConfig->ePresentMode) {
                case VK_PRESENT_MODE_IMMEDIATE_KHR: currentMode = 0; break;
                case VK_PRESENT_MODE_FIFO_KHR: currentMode = 1; break;
                case VK_PRESENT_MODE_MAILBOX_KHR: currentMode = 2; break;
                case VK_PRESENT_MODE_FIFO_RELAXED_KHR: currentMode = 3; break;
                default: currentMode = 0; break;
            }
            if (ImGui::Combo("VSync Mode", &currentMode, presentModes, 4)) {
                switch (currentMode) {
                    case 0: pConfig->ePresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; break;
                    case 1: pConfig->ePresentMode = VK_PRESENT_MODE_FIFO_KHR; break;
                    case 2: pConfig->ePresentMode = VK_PRESENT_MODE_MAILBOX_KHR; break;
                    case 3: pConfig->ePresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR; break;
                }
                pConfig->bSwapchainDirty = true;
            }
            
            // GPU Culling
            if (ImGui::Checkbox("GPU Frustum Culling", &pConfig->bEnableGPUCulling)) {
                // Will take effect next frame
            }
            
            // Back face culling
            if (ImGui::Checkbox("Back Face Culling", &pConfig->bCullBackFaces)) {
                // Will take effect on pipeline rebuild
            }
            
            ImGui::Spacing();
            ImGui::Spacing();
            
            // Camera section
            ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "Camera");
            ImGui::Separator();
            
            // FOV
            float fovDegrees = pConfig->fCameraFovYRad * 57.2958f;  // rad to deg
            if (ImGui::SliderFloat("Field of View", &fovDegrees, 30.0f, 120.0f, "%.0f deg")) {
                pConfig->fCameraFovYRad = fovDegrees * 0.0174533f;  // deg to rad
            }
            
            // Camera speed
            ImGui::SliderFloat("Camera Speed", &pConfig->fPanSpeed, 1.0f, 50.0f, "%.1f");
            
            ImGui::Spacing();
            ImGui::Spacing();
            
            // Debug section
            ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "Debug");
            ImGui::Separator();
            
            ImGui::Checkbox("Show Light Debug", &pConfig->bShowLightDebug);
            
            ImGui::EndChild();
        }
    }
    ImGui::End();
}
