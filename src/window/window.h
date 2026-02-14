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
    Window(uint32_t width, uint32_t height, const char* title = "Vulkan App");
    ~Window();

    /* Create Vulkan surface (call after VkInstance is created). */
    void CreateSurface(VkInstance instance);
    void DestroySurface(VkInstance instance);

    /* Process events; returns true if quit requested. */
    bool PollEvents();

    void SetSize(uint32_t width, uint32_t height);
    void SetFullscreen(bool fullscreen);
    void SetTitle(const char* title);

    VkSurfaceKHR GetSurface() const { return m_surface; }
    SDL_Window* GetSDLWindow() { return m_pWindow; }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    /** Current drawable size in pixels (for swapchain extent). */
    void GetDrawableSize(uint32_t* outWidth, uint32_t* outHeight) const;

    bool GetFramebufferResized() const { return m_bFramebufferResized; }
    void SetFramebufferResized(bool b) { m_bFramebufferResized = b; }
    bool GetWindowMinimized() const { return m_bWindowMinimized; }

private:
    SDL_Window* m_pWindow = static_cast<SDL_Window*>(nullptr);
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    uint32_t m_width = static_cast<uint32_t>(0);
    uint32_t m_height = static_cast<uint32_t>(0);
    bool m_bFramebufferResized = static_cast<bool>(false);
    bool m_bWindowMinimized = static_cast<bool>(false);
};
