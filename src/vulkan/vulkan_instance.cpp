#include "vulkan_instance.h"
#include "vulkan_utils.h"
#include <cstring>
#include <stdexcept>

void VulkanInstance::CheckExtensionsAvailable(const char* const* pExtensionNames, uint32_t extensionCount) {
    VulkanUtils::LogTrace("CheckInstanceExtensionsAvailable");
    uint32_t availableCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, nullptr);
    std::vector<VkExtensionProperties> available(availableCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, available.data());
    for (uint32_t i = 0; i < extensionCount; ++i) {
        const char* pName = pExtensionNames[i];
        bool bFound = false;
        for (const auto& prop : available) {
            if (std::strcmp(pName, prop.extensionName) == 0) {
                bFound = true;
                break;
            }
        }
        if (bFound == false) {
            VulkanUtils::LogErr("Instance extension not available: {}", pName);
            throw std::runtime_error("Required Vulkan instance extension not available");
        }
    }
}

void VulkanInstance::Create(const char* const* pExtensionNames, uint32_t extensionCount) {
    VulkanUtils::LogTrace("CreateVulkanInstance");
    if (pExtensionNames == nullptr || extensionCount == 0) {
        VulkanUtils::LogErr("No Vulkan instance extensions provided");
        throw std::runtime_error("No Vulkan instance extensions provided");
    }
    CheckExtensionsAvailable(pExtensionNames, extensionCount);

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "Custom Vulkan App",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Custom Vulkan Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_4,
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = pExtensionNames,
    };

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateInstance failed: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

void VulkanInstance::Destroy() {
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

VulkanInstance::~VulkanInstance() {
    Destroy();
}
