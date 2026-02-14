/*
 * Window — SDL window and Vulkan surface. Events set flags (resized, minimized) for the app to react.
 */
#include "window.h"
#include "vulkan_utils.h"
#include <stdexcept>

Window::Window(uint32_t width, uint32_t height, const char* title) : m_width(width), m_height(height) {
    VulkanUtils::LogTrace("Window constructor");
    SDL_SetHint(SDL_HINT_APP_ID, "VulkanApp");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        VulkanUtils::LogErr("SDL_Init failed: {}", SDL_GetError());
        throw std::runtime_error(SDL_GetError());
    }
    m_pWindow = SDL_CreateWindow(title, static_cast<int>(width), static_cast<int>(height),
                                 SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!m_pWindow) {
        VulkanUtils::LogErr("SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        throw std::runtime_error(SDL_GetError());
    }
    SDL_SetWindowPosition(m_pWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(m_pWindow);
    SDL_RaiseWindow(m_pWindow);
}

Window::~Window() {
    VulkanUtils::LogTrace("Window destructor");
    if (m_pWindow) {
        SDL_DestroyWindow(m_pWindow);
        m_pWindow = nullptr;
    }
    SDL_Quit();
}

void Window::CreateSurface(VkInstance instance) {
    VulkanUtils::LogTrace("CreateSurface");
    if (instance == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("CreateSurface: invalid instance");
        throw std::runtime_error("CreateSurface: invalid instance");
    }
    if (m_surface != VK_NULL_HANDLE) {
        VulkanUtils::LogErr("CreateSurface: surface already created");
        throw std::runtime_error("CreateSurface: surface already created");
    }
    if (!SDL_Vulkan_CreateSurface(m_pWindow, instance, nullptr, &m_surface)) {
        VulkanUtils::LogErr("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        throw std::runtime_error(SDL_GetError());
    }
}

void Window::DestroySurface(VkInstance instance) {
    if (m_surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
}

void Window::SetSize(uint32_t width, uint32_t height) {
    if (!m_pWindow) return;
    SDL_SetWindowSize(m_pWindow, static_cast<int>(width), static_cast<int>(height));
    m_width = width;
    m_height = height;
    m_bFramebufferResized = true;
}

void Window::SetFullscreen(bool fullscreen) {
    if (!m_pWindow) return;
    SDL_SetWindowFullscreen(m_pWindow, fullscreen);
    m_bFramebufferResized = true;
}

void Window::SetTitle(const char* title) {
    if (m_pWindow && title)
        SDL_SetWindowTitle(m_pWindow, title);
}

void Window::GetDrawableSize(uint32_t* outWidth, uint32_t* outHeight) const {
    if (!m_pWindow) {
        if (outWidth)  *outWidth  = 0;
        if (outHeight) *outHeight = 0;
        return;
    }
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(m_pWindow, &w, &h);
    if (outWidth)  *outWidth  = (w > 0) ? static_cast<uint32_t>(w) : m_width;
    if (outHeight) *outHeight = (h > 0) ? static_cast<uint32_t>(h) : m_height;
}

bool Window::PollEvents() {
    SDL_Event evt;
    while (SDL_PollEvent(&evt)) {
        switch (evt.type) {
            case SDL_EVENT_QUIT:
                return true;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                m_bFramebufferResized = true;
                int w = 0, h = 0;
                if (SDL_GetWindowSizeInPixels(m_pWindow, &w, &h)) {
                    if (w > 0) m_width  = static_cast<uint32_t>(w);
                    if (h > 0) m_height = static_cast<uint32_t>(h);
                }
                break;
            }
            case SDL_EVENT_WINDOW_MINIMIZED:
                m_bWindowMinimized = true;
                break;
            case SDL_EVENT_WINDOW_MAXIMIZED:
            case SDL_EVENT_WINDOW_RESTORED:
                m_bWindowMinimized = false;
                m_bFramebufferResized = true;
                break;
            case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
            case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
                m_bFramebufferResized = true;
                break;
            default:
                break;
        }
    }
    return false;
}
