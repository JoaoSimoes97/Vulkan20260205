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
    for (uint32_t lIdx = static_cast<uint32_t>(0); lIdx < extensionCount; ++lIdx) {
        const char* pName = pExtensionNames[lIdx];
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
    if ((pExtensionNames == nullptr) || (extensionCount == 0)) {
        VulkanUtils::LogErr("No Vulkan instance extensions provided");
        throw std::runtime_error("No Vulkan instance extensions provided");
    }
    CheckExtensionsAvailable(pExtensionNames, extensionCount);

    VkApplicationInfo appInfo = {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,   /* Application info struct. */
        .pNext              = nullptr,                              /* No extension chain. */
        .pApplicationName   = "Custom Vulkan App",                  /* App name for driver/debug. */
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),             /* App version. */
        .pEngineName        = "Custom Vulkan Engine",               /* Engine name. */
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),             /* Engine version. */
        .apiVersion         = VK_API_VERSION_1_4,                   /* Requested Vulkan API version. */
    };

    VkInstanceCreateInfo createInfo = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,  /* Instance create. */
        .pNext                   = nullptr,                                 /* No extension chain. */
        .flags                   = 0,                                       /* No create flags. */
        .pApplicationInfo        = &appInfo,                                /* App/engine/API version. */
        .enabledLayerCount       = 0,                                       /* No validation or other layers. */
        .ppEnabledLayerNames     = nullptr,                                 /* No enabled layers. */
        .enabledExtensionCount   = extensionCount,                          /* e.g. surface, platform. */
        .ppEnabledExtensionNames = pExtensionNames,                         /* Provided extension names. */
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
