#pragma once

#include <cstdint>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

/*
 * Platform window and Vulkan surface. Owns SDL init, window, and surface.
 * Events (resize, minimize, etc.) set flags for the app to react (e.g. recreate swapchain).
 * Future: multiple windows, each with its own surface.
 */
class Window {
public:
    Window(uint32_t lWidth_ic, uint32_t lHeight_ic, const char* pTitle_ic = "Vulkan App");
    ~Window();

    /* Create Vulkan surface (call after VkInstance is created). */
    void CreateSurface(VkInstance pInstance_ic);
    void DestroySurface(VkInstance pInstance_ic);

    /* Process events; returns true if quit requested. */
    bool PollEvents();

    void SetSize(uint32_t lWidth_ic, uint32_t lHeight_ic);
    void SetFullscreen(bool bFullscreen_ic);
    void SetTitle(const char* pTitle_ic);

    VkSurfaceKHR GetSurface() const { return this->m_surface; }
    SDL_Window* GetSDLWindow() { return this->m_pWindow; }
    uint32_t GetWidth() const { return this->m_width; }
    uint32_t GetHeight() const { return this->m_height; }
    /** Current drawable size in pixels (for swapchain extent). */
    void GetDrawableSize(uint32_t* pOutWidth_out, uint32_t* pOutHeight_out) const;

    bool GetFramebufferResized() const { return this->m_bFramebufferResized; }
    void SetFramebufferResized(bool b_ic) { this->m_bFramebufferResized = b_ic; }
    bool GetWindowMinimized() const { return this->m_bWindowMinimized; }

private:
    SDL_Window* m_pWindow = static_cast<SDL_Window*>(nullptr);
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    uint32_t m_width = static_cast<uint32_t>(0);
    uint32_t m_height = static_cast<uint32_t>(0);
    bool m_bFramebufferResized = static_cast<bool>(false);
    bool m_bWindowMinimized = static_cast<bool>(false);
};
