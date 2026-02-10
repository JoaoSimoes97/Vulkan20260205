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

    if ((VulkanUtils::ENABLE_VALIDATION_LAYERS == true) && (VulkanUtils::CheckValidationLayerSupport() == false)) {
        VulkanUtils::LogErr("Validation layers requested, but not available");
        throw std::runtime_error("Validation layers requested, but not available");
    }

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
    if (VulkanUtils::ENABLE_VALIDATION_LAYERS == true) {
        VulkanUtils::PopulateDebugMessengerCreateInfo(debugCreateInfo);
    }

    VkApplicationInfo appInfo = {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,   /* Application info struct. */
        .pNext              = nullptr,                              /* No extension chain. */
        .pApplicationName   = "Custom Vulkan App",                  /* App name for driver/debug. */
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),             /* App version. */
        .pEngineName        = "Custom Vulkan Engine",               /* Engine name. */
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),             /* Engine version. */
        .apiVersion         = VK_API_VERSION_1_4,                    /* Requested Vulkan API version. */
    };

    VkInstanceCreateInfo createInfo = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext                   = VulkanUtils::ENABLE_VALIDATION_LAYERS ? &debugCreateInfo : nullptr,
        .flags                   = 0,
        .pApplicationInfo        = &appInfo,
        .enabledLayerCount       = VulkanUtils::ENABLE_VALIDATION_LAYERS ? static_cast<uint32_t>(VulkanUtils::VALIDATION_LAYERS.size()) : 0u,
        .ppEnabledLayerNames     = VulkanUtils::ENABLE_VALIDATION_LAYERS ? VulkanUtils::VALIDATION_LAYERS.data() : nullptr,
        .enabledExtensionCount   = extensionCount,
        .ppEnabledExtensionNames = pExtensionNames,
    };

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateInstance failed: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    if (VulkanUtils::ENABLE_VALIDATION_LAYERS == true) {
        VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = {};
        VulkanUtils::PopulateDebugMessengerCreateInfo(messengerCreateInfo);
        result = VulkanUtils::CreateDebugUtilsMessengerEXT(m_instance, &messengerCreateInfo, nullptr, &m_debugMessenger);
        if (result != VK_SUCCESS) {
            VulkanUtils::LogErr("Failed to set up debug messenger");
            Destroy();
            throw std::runtime_error("Failed to set up debug messenger");
        }
    }
}

void VulkanInstance::Destroy() {
    if ((VulkanUtils::ENABLE_VALIDATION_LAYERS == true) && (m_debugMessenger != VK_NULL_HANDLE)) {
        VulkanUtils::DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

VulkanInstance::~VulkanInstance() {
    Destroy();
}
