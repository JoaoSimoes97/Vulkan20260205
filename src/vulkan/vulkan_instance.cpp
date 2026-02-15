#include "vulkan_instance.h"
#include "vulkan_utils.h"
#include <cstring>
#include <stdexcept>

void VulkanInstance::CheckExtensionsAvailable(const char* const* pExtensionNames_ic, uint32_t lExtensionCount_ic) {
    VulkanUtils::LogTrace("CheckInstanceExtensionsAvailable");
    uint32_t lAvailableCount = static_cast<uint32_t>(0);
    vkEnumerateInstanceExtensionProperties(nullptr, &lAvailableCount, nullptr);
    std::vector<VkExtensionProperties> vecAvailable(lAvailableCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &lAvailableCount, vecAvailable.data());
    for (uint32_t lIdx = static_cast<uint32_t>(0); lIdx < lExtensionCount_ic; ++lIdx) {
        const char* pName = pExtensionNames_ic[lIdx];
        bool bFound = false;
        for (const auto& stProp : vecAvailable) {
            if (std::strcmp(pName, stProp.extensionName) == 0) {
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

void VulkanInstance::Create(const char* const* pExtensionNames_ic, uint32_t lExtensionCount_ic) {
    VulkanUtils::LogTrace("CreateVulkanInstance");
    if ((pExtensionNames_ic == nullptr) || (lExtensionCount_ic == 0)) {
        VulkanUtils::LogErr("No Vulkan instance extensions provided");
        throw std::runtime_error("No Vulkan instance extensions provided");
    }
    CheckExtensionsAvailable(pExtensionNames_ic, lExtensionCount_ic);

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
        .enabledExtensionCount   = lExtensionCount_ic,
        .ppEnabledExtensionNames = pExtensionNames_ic,
    };

    VkResult result = vkCreateInstance(&createInfo, nullptr, &this->m_instance);
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateInstance failed: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    if (VulkanUtils::ENABLE_VALIDATION_LAYERS == true) {
        VkDebugUtilsMessengerCreateInfoEXT stMessengerCreateInfo = {};
        VulkanUtils::PopulateDebugMessengerCreateInfo(stMessengerCreateInfo);
        result = VulkanUtils::CreateDebugUtilsMessengerEXT(this->m_instance, &stMessengerCreateInfo, nullptr, &this->m_debugMessenger);
        if (result != VK_SUCCESS) {
            VulkanUtils::LogErr("Failed to set up debug messenger");
            Destroy();
            throw std::runtime_error("Failed to set up debug messenger");
        }
    }
}

void VulkanInstance::Destroy() {
    if ((VulkanUtils::ENABLE_VALIDATION_LAYERS == true) && (this->m_debugMessenger != VK_NULL_HANDLE)) {
        VulkanUtils::DestroyDebugUtilsMessengerEXT(this->m_instance, this->m_debugMessenger, nullptr);
        this->m_debugMessenger = VK_NULL_HANDLE;
    }
    if (this->m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(this->m_instance, nullptr);
        this->m_instance = VK_NULL_HANDLE;
    }
}

VulkanInstance::~VulkanInstance() {
    Destroy();
}
