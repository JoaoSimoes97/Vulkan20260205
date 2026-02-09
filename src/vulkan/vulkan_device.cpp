#include "vulkan_device.h"
#include "vulkan_utils.h"
#include <stdexcept>
#include <vector>

static constexpr const char* DEVICE_EXTENSION_SWAPCHAIN = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

QueueFamilyIndices VulkanDevice::FindQueueFamilyIndices(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    QueueFamilyIndices indices = {};
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0)
        return indices;
    std::vector<VkQueueFamilyProperties> props(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, props.data());

    for (uint32_t lIdx = static_cast<uint32_t>(0); lIdx < queueFamilyCount; ++lIdx) {
        if ((props[lIdx].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            indices.graphicsFamily = lIdx;
        if (surface != VK_NULL_HANDLE) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, lIdx, surface, &presentSupport);
            if (presentSupport == VK_TRUE)
                indices.presentFamily = lIdx;
        }
    }
    return indices;
}

uint32_t VulkanDevice::RateSuitability(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceProperties& props) {
    QueueFamilyIndices indices = FindQueueFamilyIndices(physicalDevice, VK_NULL_HANDLE);
    if (indices.graphicsFamily == QUEUE_FAMILY_IGNORED)
        return 0;

    uint32_t score = 0;
    switch (props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   score += 1000; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:  score += 100;  break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:     score += 50;   break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:             score += 10;   break;
        default: break;
    }

    /* Query supported features for suitability check. */
    VkPhysicalDeviceFeatures features = {};
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);
    if (features.geometryShader == VK_FALSE)
        return 0;
    return score;
}

void VulkanDevice::Create(VkInstance instance, VkSurfaceKHR surface) {
    VulkanUtils::LogTrace("VulkanDevice::Create");
    if (instance == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("VulkanDevice::Create: invalid instance");
        throw std::runtime_error("VulkanDevice::Create: invalid instance");
    }
    m_instance = instance;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        VulkanUtils::LogErr("No Vulkan physical devices found");
        throw std::runtime_error("No Vulkan physical devices found");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    uint32_t bestScore = 0;
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    /* Properties of the chosen physical device (name, type, etc.). */
    VkPhysicalDeviceProperties bestProps = {};

    for (VkPhysicalDevice dev : devices) {
        /* Properties of current candidate device. */
        VkPhysicalDeviceProperties props = {};
        vkGetPhysicalDeviceProperties(dev, &props);
        m_queueFamilyIndices = FindQueueFamilyIndices(dev, surface);
        uint32_t score = RateSuitability(dev, props);
        VulkanUtils::LogInfo("Physical device: {} - Score: {}", props.deviceName, score);
        if (score > bestScore) {
            bestScore = score;
            bestDevice = dev;
            bestProps = props;
        }
    }

    if (bestDevice == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("No suitable Vulkan physical device found");
        throw std::runtime_error("No suitable Vulkan physical device found");
    }

    m_physicalDevice = bestDevice;
    m_queueFamilyIndices = FindQueueFamilyIndices(bestDevice, surface);
    VulkanUtils::LogInfo("Best physical device: {} - Score: {}", bestProps.deviceName, bestScore);

    if (m_queueFamilyIndices.graphicsFamily == QUEUE_FAMILY_IGNORED) {
        VulkanUtils::LogErr("Graphics queue family not found");
        throw std::runtime_error("Graphics queue family not found");
    }

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType             = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,    /* Queue create. */
        .pNext             = nullptr,                                       /* No extension chain. */
        .flags             = 0,                                             /* No flags. */
        .queueFamilyIndex  = m_queueFamilyIndices.graphicsFamily,           /* Graphics queue family. */
        .queueCount        = 1,                                             /* Single queue. */
        .pQueuePriorities  = &queuePriority,                                /* Priority 1.0. */
    };

    VkPhysicalDeviceFeatures deviceFeatures = {};
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &deviceFeatures);
    if (deviceFeatures.geometryShader == VK_FALSE) {
        VulkanUtils::LogErr("Physical device does not support geometry shaders");
        throw std::runtime_error("Physical device does not support geometry shaders");
    }

    VkDeviceCreateInfo createInfo = {
        .sType                 = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,      /* Logical device create. */
        .pNext                 = nullptr,                                   /* No extension chain. */
        .flags                 = 0,                                         /* No flags. */
        .queueCreateInfoCount  = 1,                                         /* One queue create info. */
        .pQueueCreateInfos     = &queueCreateInfo,                          /* Graphics queue. */
        .enabledLayerCount     = 0,                                         /* No enabled layers. */
        .ppEnabledLayerNames   = nullptr,                                   /* No enabled layers. */
        .enabledExtensionCount = 1,                                         /* Swapchain only. */
        .ppEnabledExtensionNames = &DEVICE_EXTENSION_SWAPCHAIN,             /* VK_KHR_swapchain. */
        .pEnabledFeatures      = &deviceFeatures,                           /* e.g. geometry shader. */
    };

    VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_logicalDevice);
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateDevice failed: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to create logical device");
    }

    constexpr uint32_t QUEUE_INDEX_FIRST = 0;
    vkGetDeviceQueue(m_logicalDevice, m_queueFamilyIndices.graphicsFamily, QUEUE_INDEX_FIRST, &m_graphicsQueue);
}

void VulkanDevice::Destroy() {
    if (m_logicalDevice != VK_NULL_HANDLE) {
        vkDestroyDevice(m_logicalDevice, nullptr);
        m_logicalDevice = VK_NULL_HANDLE;
    }
    m_physicalDevice = VK_NULL_HANDLE;
    m_graphicsQueue = VK_NULL_HANDLE;
    m_queueFamilyIndices = {};
    m_instance = VK_NULL_HANDLE;
}

VulkanDevice::~VulkanDevice() {
    Destroy();
}
