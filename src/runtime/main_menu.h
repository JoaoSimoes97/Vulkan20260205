/*
 * MainMenu - Front page shown on startup before any level is loaded.
 * 
 * Provides:
 * - Level selection from discovered levels
 * - Settings access
 * - Quit option
 * 
 * This is the entry point UI in Release builds.
 */
#pragma once

#include "ui/imgui_base.h"
#include "scene/level_selector.h"
#include <cstdint>
#include <string>
#include <functional>

struct VulkanConfig;

/**
 * Main menu state enumeration.
 */
enum class MainMenuState {
    Main,       // Main menu with Play/Settings/Quit
    LevelSelect,// Level selection submenu
    Settings    // Settings submenu
};

/**
 * MainMenu - Full-screen main menu overlay.
 * 
 * Shows a centered menu with options for level selection, settings, and quit.
 * Designed to be shown before any level is loaded.
 */
class MainMenu : public ImGuiBase {
public:
    MainMenu() = default;
    ~MainMenu() override = default;

    /**
     * Initialize the main menu.
     */
    void Init(
        SDL_Window* pWindow,
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        uint32_t graphicsQueueFamily,
        VkQueue graphicsQueue,
        VkRenderPass renderPass,
        uint32_t imageCount
    );

    /**
     * Draw the main menu. Call between BeginFrame() and EndFrame().
     * @param pConfig Config for settings menu.
     */
    void Draw(VulkanConfig* pConfig = nullptr);

    /**
     * Check if main menu is visible.
     */
    bool IsVisible() const { return m_bVisible; }
    
    /**
     * Set main menu visibility.
     */
    void SetVisible(bool bVisible) { m_bVisible = bVisible; }

    /**
     * Toggle main menu visibility (for ESC key).
     */
    void ToggleVisible() { m_bVisible = !m_bVisible; }

    /**
     * Set level selector for level list.
     */
    void SetLevelSelector(LevelSelector* pSelector) { m_pLevelSelector = pSelector; }

    /**
     * Set quit callback.
     */
    void SetQuitCallback(std::function<void()> callback) { m_quitCallback = std::move(callback); }

    /**
     * Check if a level load was requested (auto-hides menu after selection).
     */
    bool WasLevelLoadRequested() const { return m_bLevelLoadRequested; }
    
    /**
     * Clear the level load request flag (call after actually loading).
     */
    void ClearLevelLoadRequest() { m_bLevelLoadRequested = false; }

    /**
     * Check if quit was requested.
     */
    bool WasQuitRequested() const { return m_bQuitRequested; }

    /**
     * Get current menu state.
     */
    MainMenuState GetState() const { return m_state; }

    /**
     * Set whether a level is currently loaded (shows Resume button if true).
     */
    void SetLevelLoaded(bool bLoaded) { m_bLevelLoaded = bLoaded; }
    bool IsLevelLoaded() const { return m_bLevelLoaded; }

protected:
    void DrawContent() override;

private:
    void DrawMainPage();
    void DrawLevelSelectPage();
    void DrawSettingsPage(VulkanConfig* pConfig);
    void DrawLevelCard(const LevelInfo& level, int index, bool isSelected);

    bool m_bVisible = true;
    MainMenuState m_state = MainMenuState::Main;
    
    LevelSelector* m_pLevelSelector = nullptr;
    VulkanConfig* m_pCurrentConfig = nullptr;
    
    std::function<void()> m_quitCallback;
    
    bool m_bLevelLoadRequested = false;
    bool m_bQuitRequested = false;
    bool m_bLevelLoaded = false;  // True when a level has been loaded (shows Resume button)
    
    // Animation/transition state
    float m_fadeAlpha = 1.0f;
};
