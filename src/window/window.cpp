#include "window.h"
#include "vulkan_utils.h"
#include <stdexcept>

Window::Window(uint32_t lWidth, uint32_t lHeight, const char* pTitle) : m_width(lWidth), m_height(lHeight) {
    VulkanUtils::LogTrace("Window constructor");
    SDL_SetHint(SDL_HINT_APP_ID, "VulkanApp");
    if (SDL_Init(SDL_INIT_VIDEO) == false) {
        VulkanUtils::LogErr("SDL_Init failed: {}", SDL_GetError());
        throw std::runtime_error(SDL_GetError());
    }
    this->m_pWindow = SDL_CreateWindow(pTitle, static_cast<int>(lWidth), static_cast<int>(lHeight),
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
    /* Surface must be destroyed by caller with DestroySurface(instance) before instance is destroyed. */
    if (this->m_pWindow != nullptr) {
        SDL_DestroyWindow(this->m_pWindow);
        this->m_pWindow = static_cast<SDL_Window*>(nullptr);
    }
    SDL_Quit();
}

void Window::CreateSurface(VkInstance instance) {
    VulkanUtils::LogTrace("CreateSurface");
    if (instance == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("CreateSurface: invalid instance");
        throw std::runtime_error("CreateSurface: invalid instance");
    }
    if (this->m_surface != VK_NULL_HANDLE) {
        VulkanUtils::LogErr("CreateSurface: surface already created");
        throw std::runtime_error("CreateSurface: surface already created");
    }
    const bool bOk = SDL_Vulkan_CreateSurface(this->m_pWindow, instance, nullptr, &this->m_surface);
    if (bOk == false) {
        VulkanUtils::LogErr("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        throw std::runtime_error(SDL_GetError());
    }
}

void Window::DestroySurface(VkInstance instance) {
    if ((this->m_surface != VK_NULL_HANDLE) && (instance != VK_NULL_HANDLE)) {
        vkDestroySurfaceKHR(instance, this->m_surface, nullptr);
        this->m_surface = VK_NULL_HANDLE;
    }
}

void Window::SetSize(uint32_t lWidth, uint32_t lHeight) {
    if (this->m_pWindow == nullptr)
        return;
    SDL_SetWindowSize(this->m_pWindow, static_cast<int>(lWidth), static_cast<int>(lHeight));
    this->m_width = lWidth;
    this->m_height = lHeight;
    this->m_bFramebufferResized = static_cast<bool>(true);
}

void Window::SetFullscreen(bool bFullscreen) {
    if (this->m_pWindow == nullptr)
        return;
    SDL_SetWindowFullscreen(this->m_pWindow, bFullscreen);
    this->m_bFramebufferResized = static_cast<bool>(true);
}

void Window::SetTitle(const char* pTitle) {
    if ((this->m_pWindow != nullptr) && (pTitle != nullptr))
        SDL_SetWindowTitle(this->m_pWindow, pTitle);
}

void Window::GetDrawableSize(uint32_t* pOutWidth, uint32_t* pOutHeight) const {
    if (this->m_pWindow == nullptr) {
        if (pOutWidth != nullptr) *pOutWidth = static_cast<uint32_t>(0);
        if (pOutHeight != nullptr) *pOutHeight = static_cast<uint32_t>(0);
        return;
    }
    int iW = static_cast<int>(0);
    int iH = static_cast<int>(0);
    SDL_GetWindowSizeInPixels(this->m_pWindow, &iW, &iH);
    if (pOutWidth != nullptr) *pOutWidth = (iW > 0) ? static_cast<uint32_t>(iW) : this->m_width;
    if (pOutHeight != nullptr) *pOutHeight = (iH > 0) ? static_cast<uint32_t>(iH) : this->m_height;
}

bool Window::PollEvents() {
    SDL_Event evt;
    while (SDL_PollEvent(&evt)) {
        switch (evt.type) {
            case SDL_EVENT_QUIT:
                return true;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                this->m_bFramebufferResized = static_cast<bool>(true);
                int iW = static_cast<int>(0);
                int iH = static_cast<int>(0);
                if (SDL_GetWindowSizeInPixels(this->m_pWindow, &iW, &iH) == true) {
                    this->m_width = (iW > 0) ? static_cast<uint32_t>(iW) : this->m_width;
                    this->m_height = (iH > 0) ? static_cast<uint32_t>(iH) : this->m_height;
                }
                break;
            }
            case SDL_EVENT_WINDOW_MINIMIZED:
                this->m_bWindowMinimized = static_cast<bool>(true);
                break;
            case SDL_EVENT_WINDOW_MAXIMIZED:
            case SDL_EVENT_WINDOW_RESTORED:
                this->m_bWindowMinimized = static_cast<bool>(false);
                this->m_bFramebufferResized = static_cast<bool>(true);
                break;
            case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
                this->m_bFramebufferResized = static_cast<bool>(true);
                break;
            case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
                this->m_bFramebufferResized = static_cast<bool>(true);
                break;
            default:
                break;
        }
    }
    return false;
}
