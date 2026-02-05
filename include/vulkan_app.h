#pragma once

#include <cstdint>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

/* Sentinel for “queue family not found”. */
constexpr uint32_t QUEUE_FAMILY_IGNORED = UINT32_MAX;

struct QueueFamilyIndices {
    uint32_t graphicsFamily = QUEUE_FAMILY_IGNORED;
    uint32_t presentFamily  = QUEUE_FAMILY_IGNORED;
};

/*
 * Rebuild cases: docs/vulkan/swapchain-rebuild-cases.md. Tutorial order: docs/vulkan/tutorial-order.md.
 * Event loop sets bFramebufferResized and bWindowMinimized; Vulkan must recreateSwapChain when bFramebufferResized and skip draw when bWindowMinimized.
 */

class VulkanApp {
public:
    VulkanApp();
    ~VulkanApp();

    void Run();

private:
    SDL_Window* pWindow = static_cast<SDL_Window*>(nullptr);
    bool bFramebufferResized = static_cast<bool>(false);
    bool bWindowMinimized = static_cast<bool>(false);

    VkInstance pVulkanInstance = static_cast<VkInstance>(nullptr);

    struct stMainDevice_t {
        VkPhysicalDevice pPhysicalDevice = static_cast<VkPhysicalDevice>(nullptr);
        VkDevice pLogicalDevice = static_cast<VkDevice>(nullptr);
        QueueFamilyIndices stQueueFamilyIndices = {};
        VkQueue graphicsQueue = static_cast<VkQueue>(VK_NULL_HANDLE);  /* From vkGetDeviceQueue after device creation. */
    } stMainDevice = {};
    
    void InitWindow();
    void InitVulkan();
    void MainLoop();
    void Cleanup();
    void DrawFrame();
    void CreateVulkanInstance();
    void CheckInstanceExtensionsAvailable(const char* const* pExtensionNames, uint32_t lExtensionCount);
    void GetPhysicalDevice();
    uint32_t RatePhysicalDeviceSuitability(VkPhysicalDevice pPhysicalDevice, const VkPhysicalDeviceProperties& stProperties);
    QueueFamilyIndices FindQueueFamilyIndices(VkPhysicalDevice pPhysicalDevice);
    void GetQueueFamilies(VkPhysicalDevice pPhysicalDevice);
    void CreateLogicalDevice();
};
