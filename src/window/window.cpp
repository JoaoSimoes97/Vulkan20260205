/*
 * Window — SDL window and Vulkan surface. Events set flags (resized, minimized) for the app to react.
 */
#include "window.h"
#include "vulkan_utils.h"
#include <stdexcept>
#include <string>

Window::Window(uint32_t lWidth_ic, uint32_t lHeight_ic, const char* pTitle_ic) : m_width(lWidth_ic), m_height(lHeight_ic) {
    VulkanUtils::LogTrace("Window constructor");
    SDL_SetHint(SDL_HINT_APP_ID, "VulkanApp");
    /* SDL3: SDL_Init returns true on success, false on failure (unlike SDL2 which used 0 on success). So throw when false. */
    if (SDL_Init(SDL_INIT_VIDEO) == false) {
        const char* pErr = SDL_GetError();
        const char* pMsg = ((pErr != nullptr) && (pErr[0] != '\0')) ? pErr : "no display or video subsystem";
        VulkanUtils::LogErr("SDL_Init failed: {}", pMsg);
        throw std::runtime_error(pMsg);
    }
    this->m_pWindow = SDL_CreateWindow(pTitle_ic, static_cast<int>(lWidth_ic), static_cast<int>(lHeight_ic),
                                 SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (this->m_pWindow == nullptr) {
        VulkanUtils::LogErr("SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        throw std::runtime_error(SDL_GetError());
    }
    SDL_SetWindowPosition(this->m_pWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(this->m_pWindow);
    SDL_RaiseWindow(this->m_pWindow);
}

Window::~Window() {
    VulkanUtils::LogTrace("Window destructor");
    if (this->m_pWindow != nullptr) {
        SDL_DestroyWindow(this->m_pWindow);
        this->m_pWindow = nullptr;
    }
    SDL_Quit();
}

void Window::CreateSurface(VkInstance pInstance_ic) {
    VulkanUtils::LogTrace("CreateSurface");
    if (pInstance_ic == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("CreateSurface: invalid instance");
        throw std::runtime_error("CreateSurface: invalid instance");
    }
    if (this->m_surface != VK_NULL_HANDLE) {
        VulkanUtils::LogErr("CreateSurface: surface already created");
        throw std::runtime_error("CreateSurface: surface already created");
    }
    if (SDL_Vulkan_CreateSurface(this->m_pWindow, pInstance_ic, nullptr, &this->m_surface) == false) {
        VulkanUtils::LogErr("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        throw std::runtime_error(SDL_GetError());
    }
}

void Window::DestroySurface(VkInstance pInstance_ic) {
    if ((this->m_surface != VK_NULL_HANDLE) && (pInstance_ic != VK_NULL_HANDLE)) {
        vkDestroySurfaceKHR(pInstance_ic, this->m_surface, nullptr);
        this->m_surface = VK_NULL_HANDLE;
    }
}

void Window::SetSize(uint32_t lWidth_ic, uint32_t lHeight_ic) {
    if (this->m_pWindow == nullptr) return;
    SDL_SetWindowSize(this->m_pWindow, static_cast<int>(lWidth_ic), static_cast<int>(lHeight_ic));
    this->m_width = lWidth_ic;
    this->m_height = lHeight_ic;
    this->m_bFramebufferResized = true;
}

void Window::SetFullscreen(bool bFullscreen_ic) {
    if (this->m_pWindow == nullptr) return;
    SDL_SetWindowFullscreen(this->m_pWindow, bFullscreen_ic);
    this->m_bFramebufferResized = true;
}

void Window::SetTitle(const char* pTitle_ic) {
    if ((this->m_pWindow != nullptr) && (pTitle_ic != nullptr))
        SDL_SetWindowTitle(this->m_pWindow, pTitle_ic);
}

void Window::GetDrawableSize(uint32_t* pOutWidth_out, uint32_t* pOutHeight_out) const {
    if (this->m_pWindow == nullptr) {
        if (pOutWidth_out != nullptr)  *pOutWidth_out  = 0;
        if (pOutHeight_out != nullptr) *pOutHeight_out = 0;
        return;
    }
    int iW = 0;
    int iH = 0;
    SDL_GetWindowSizeInPixels(this->m_pWindow, &iW, &iH);
    if (pOutWidth_out != nullptr)  *pOutWidth_out  = (iW > 0) ? static_cast<uint32_t>(iW) : this->m_width;
    if (pOutHeight_out != nullptr) *pOutHeight_out = (iH > 0) ? static_cast<uint32_t>(iH) : this->m_height;
}

bool Window::PollEvents() {
    SDL_Event evt;
    while (SDL_PollEvent(&evt)) {
        switch (evt.type) {
            case SDL_EVENT_QUIT:
                return true;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                this->m_bFramebufferResized = true;
                int iW = 0;
                int iH = 0;
                if (SDL_GetWindowSizeInPixels(this->m_pWindow, &iW, &iH) == true) {
                    if (iW > 0) this->m_width  = static_cast<uint32_t>(iW);
                    if (iH > 0) this->m_height = static_cast<uint32_t>(iH);
                }
                break;
            }
            case SDL_EVENT_WINDOW_MINIMIZED:
                this->m_bWindowMinimized = true;
                break;
            case SDL_EVENT_WINDOW_MAXIMIZED:
            case SDL_EVENT_WINDOW_RESTORED:
                this->m_bWindowMinimized = false;
                this->m_bFramebufferResized = true;
                break;
            case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
            case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
                this->m_bFramebufferResized = true;
                break;
            default:
                break;
        }
    }
    return false;
}
