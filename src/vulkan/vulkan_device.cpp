#include "vulkan_device.h"
#include "vulkan_utils.h"
#include <stdexcept>
#include <vector>

static constexpr const char* DEVICE_EXTENSION_SWAPCHAIN = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

QueueFamilyIndices VulkanDevice::FindQueueFamilyIndices(VkPhysicalDevice pPhysicalDevice_ic, VkSurfaceKHR surface_ic) {
    QueueFamilyIndices stIndices = {};
    uint32_t lQueueFamilyCount = static_cast<uint32_t>(0);
    vkGetPhysicalDeviceQueueFamilyProperties(pPhysicalDevice_ic, &lQueueFamilyCount, nullptr);
    if (lQueueFamilyCount == 0)
        return stIndices;
    std::vector<VkQueueFamilyProperties> vecProps(lQueueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(pPhysicalDevice_ic, &lQueueFamilyCount, vecProps.data());

    for (uint32_t lIdx = static_cast<uint32_t>(0); lIdx < lQueueFamilyCount; ++lIdx) {
        if ((vecProps[lIdx].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            stIndices.graphicsFamily = lIdx;
        if (surface_ic != VK_NULL_HANDLE) {
            VkBool32 bPresentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(pPhysicalDevice_ic, lIdx, surface_ic, &bPresentSupport);
            if (bPresentSupport == VK_TRUE)
                stIndices.presentFamily = lIdx;
        }
    }
    return stIndices;
}

uint32_t VulkanDevice::RateSuitability(VkPhysicalDevice pPhysicalDevice_ic, const VkPhysicalDeviceProperties& stProps_ic) {
    QueueFamilyIndices stIndices = FindQueueFamilyIndices(pPhysicalDevice_ic, VK_NULL_HANDLE);
    if (stIndices.graphicsFamily == QUEUE_FAMILY_IGNORED)
        return static_cast<uint32_t>(0);

    uint32_t lScore = static_cast<uint32_t>(0);
    switch (stProps_ic.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   lScore += static_cast<uint32_t>(1000); break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: lScore += static_cast<uint32_t>(100); break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    lScore += static_cast<uint32_t>(50);  break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            lScore += static_cast<uint32_t>(10); break;
        default: break;
    }

    /* Query supported features for suitability check. */
    VkPhysicalDeviceFeatures stFeatures = {};
    vkGetPhysicalDeviceFeatures(pPhysicalDevice_ic, &stFeatures);
    if (stFeatures.geometryShader == VK_FALSE)
        return static_cast<uint32_t>(0);
    return lScore;
}

void VulkanDevice::Create(VkInstance pInstance_ic, VkSurfaceKHR surface_ic) {
    VulkanUtils::LogTrace("VulkanDevice::Create");
    if (pInstance_ic == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("VulkanDevice::Create: invalid instance");
        throw std::runtime_error("VulkanDevice::Create: invalid instance");
    }
    this->m_instance = pInstance_ic;

    uint32_t lDeviceCount = static_cast<uint32_t>(0);
    vkEnumeratePhysicalDevices(pInstance_ic, &lDeviceCount, nullptr);
    if (lDeviceCount == 0) {
        VulkanUtils::LogErr("No Vulkan physical devices found");
        throw std::runtime_error("No Vulkan physical devices found");
    }
    std::vector<VkPhysicalDevice> vecDevices(lDeviceCount);
    vkEnumeratePhysicalDevices(pInstance_ic, &lDeviceCount, vecDevices.data());

    uint32_t lBestScore = static_cast<uint32_t>(0);
    VkPhysicalDevice pBestDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties stBestProps = {};

    for (VkPhysicalDevice pDev : vecDevices) {
        VkPhysicalDeviceProperties stProps = {};
        vkGetPhysicalDeviceProperties(pDev, &stProps);
        this->m_queueFamilyIndices = FindQueueFamilyIndices(pDev, surface_ic);
        uint32_t lScore = RateSuitability(pDev, stProps);
        VulkanUtils::LogInfo("Physical device: {} - Score: {}", stProps.deviceName, lScore);
        if (lScore > lBestScore) {
            lBestScore = lScore;
            pBestDevice = pDev;
            stBestProps = stProps;
        }
    }

    if (pBestDevice == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("No suitable Vulkan physical device found");
        throw std::runtime_error("No suitable Vulkan physical device found");
    }

    this->m_physicalDevice = pBestDevice;
    this->m_queueFamilyIndices = FindQueueFamilyIndices(pBestDevice, surface_ic);
    
    // Query device limits
    vkGetPhysicalDeviceProperties(pBestDevice, &stBestProps);
    this->m_limits = stBestProps.limits;
    
    VulkanUtils::LogInfo("Best physical device: {} - Score: {}", stBestProps.deviceName, lBestScore);
    VulkanUtils::LogInfo("Device limits: maxDescriptorSets={}, maxBoundDescriptorSets={}, maxMemoryAllocations={}", 
        m_limits.maxDescriptorSetSamplers, m_limits.maxBoundDescriptorSets, m_limits.maxMemoryAllocationCount);

    if (this->m_queueFamilyIndices.graphicsFamily == QUEUE_FAMILY_IGNORED) {
        VulkanUtils::LogErr("Graphics queue family not found");
        throw std::runtime_error("Graphics queue family not found");
    }
    if ((surface_ic != VK_NULL_HANDLE) && (this->m_queueFamilyIndices.presentFamily == QUEUE_FAMILY_IGNORED)) {
        VulkanUtils::LogErr("Present queue family not found");
        throw std::runtime_error("Present queue family not found");
    }

    const float fQueuePriority = static_cast<float>(1.0f);
    std::vector<VkDeviceQueueCreateInfo> vecQueueCreateInfos;
    vecQueueCreateInfos.push_back({
        .sType             = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext             = nullptr,
        .flags             = 0,
        .queueFamilyIndex  = this->m_queueFamilyIndices.graphicsFamily,
        .queueCount        = static_cast<uint32_t>(1),
        .pQueuePriorities  = &fQueuePriority,
    });
    if ((surface_ic != VK_NULL_HANDLE) && (this->m_queueFamilyIndices.presentFamily != this->m_queueFamilyIndices.graphicsFamily)) {
        vecQueueCreateInfos.push_back({
            .sType             = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext             = nullptr,
            .flags             = 0,
            .queueFamilyIndex  = this->m_queueFamilyIndices.presentFamily,
            .queueCount        = static_cast<uint32_t>(1),
            .pQueuePriorities  = &fQueuePriority,
        });
    }

    VkPhysicalDeviceFeatures stDeviceFeatures = {};
    vkGetPhysicalDeviceFeatures(this->m_physicalDevice, &stDeviceFeatures);
    if (stDeviceFeatures.geometryShader == VK_FALSE) {
        VulkanUtils::LogErr("Physical device does not support geometry shaders");
        throw std::runtime_error("Physical device does not support geometry shaders");
    }

    VkDeviceCreateInfo stCreateInfo = {
        .sType                 = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .queueCreateInfoCount  = static_cast<uint32_t>(vecQueueCreateInfos.size()),
        .pQueueCreateInfos     = vecQueueCreateInfos.data(),
        .enabledLayerCount     = VulkanUtils::ENABLE_VALIDATION_LAYERS ? static_cast<uint32_t>(VulkanUtils::VALIDATION_LAYERS.size()) : static_cast<uint32_t>(0u),
        .ppEnabledLayerNames   = VulkanUtils::ENABLE_VALIDATION_LAYERS ? VulkanUtils::VALIDATION_LAYERS.data() : nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(1),
        .ppEnabledExtensionNames = &DEVICE_EXTENSION_SWAPCHAIN,
        .pEnabledFeatures      = &stDeviceFeatures,
    };

    VkResult r = vkCreateDevice(this->m_physicalDevice, &stCreateInfo, nullptr, &this->m_logicalDevice);
    if (r != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateDevice failed: {}", static_cast<int>(r));
        throw std::runtime_error("Failed to create logical device");
    }

    constexpr uint32_t QUEUE_INDEX_FIRST = 0;
    vkGetDeviceQueue(this->m_logicalDevice, this->m_queueFamilyIndices.graphicsFamily, QUEUE_INDEX_FIRST, &this->m_graphicsQueue);
    if ((surface_ic != VK_NULL_HANDLE) && (this->m_queueFamilyIndices.presentFamily != QUEUE_FAMILY_IGNORED)) {
        vkGetDeviceQueue(this->m_logicalDevice, this->m_queueFamilyIndices.presentFamily, QUEUE_INDEX_FIRST, &this->m_presentQueue);
    } else {
        this->m_presentQueue = this->m_graphicsQueue;
    }
}

void VulkanDevice::Destroy() {
    if (this->m_logicalDevice != VK_NULL_HANDLE) {
        vkDestroyDevice(this->m_logicalDevice, nullptr);
        this->m_logicalDevice = VK_NULL_HANDLE;
    }
    this->m_physicalDevice = VK_NULL_HANDLE;
    this->m_graphicsQueue = VK_NULL_HANDLE;
    this->m_presentQueue  = VK_NULL_HANDLE;
    this->m_queueFamilyIndices = {};
    this->m_instance = VK_NULL_HANDLE;
}

VulkanDevice::~VulkanDevice() {
    Destroy();
}
