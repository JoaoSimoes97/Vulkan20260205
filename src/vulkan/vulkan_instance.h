#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

/*
 * Vulkan instance: API version, layers, extensions.
 * Extension names for surface (e.g. from SDL_Vulkan_GetInstanceExtensions) are passed at creation.
 */
class VulkanInstance {
public:
    VulkanInstance() = default;
    ~VulkanInstance();

    /* Create instance with given extension names (e.g. from SDL). Throws on failure. */
    void Create(const char* const* pExtensionNames_ic, uint32_t lExtensionCount_ic);
    void Destroy();

    VkInstance Get() const { return this->m_instance; }
    bool IsValid() const { return this->m_instance != VK_NULL_HANDLE; }

private:
    static void CheckExtensionsAvailable(const char* const* pExtensionNames_ic, uint32_t lExtensionCount_ic);

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
};
