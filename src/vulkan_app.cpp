#include "vulkan_app.h"
#include "vulkan_utils.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <SDL3/SDL_stdinc.h>

constexpr int WINDOW_WIDTH = static_cast<int>(800);
constexpr int WINDOW_HEIGHT = static_cast<int>(600);

VulkanApp::VulkanApp() {
    VulkanUtils::LogTrace("VulkanApp constructor");
    this->InitWindow();
    this->InitVulkan();
}

VulkanApp::~VulkanApp() {
    VulkanUtils::LogTrace("VulkanApp destructor");
    this->Cleanup();
}

void VulkanApp::InitWindow() {
    VulkanUtils::LogTrace("InitWindow");
    SDL_SetHint(SDL_HINT_APP_ID, "VulkanApp");
    /* SDL3: SDL_Init returns true on success, false on failure (unlike SDL2 which returned 0 on success). */
    if (SDL_Init(SDL_INIT_VIDEO) == false) {
        VulkanUtils::LogErr("SDL_Init failed: {}", SDL_GetError());
        throw std::runtime_error(SDL_GetError());
    }
    this->pWindow = SDL_CreateWindow("Vulkan App", WINDOW_WIDTH, WINDOW_HEIGHT,
                                     SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (this->pWindow == nullptr) {
        VulkanUtils::LogErr("SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        throw std::runtime_error(SDL_GetError());
    }
    SDL_SetWindowPosition(this->pWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(this->pWindow);
    SDL_RaiseWindow(this->pWindow);
}

void VulkanApp::InitVulkan() {
    VulkanUtils::LogTrace("InitVulkan");

    /*  Physical device: the GPU we will use.
        Logical device: the Vulkan interface to the GPU.
        
        Physical device:
            -GPU itself.
            -Contains two important aspects:
                -Memory: When we want to allocate memory resources, it must be handled through the Physical Device.
                -Queues: Process commands submitted to GPU in FIFO order. Different queues can be used for different types of commands.
            Physical devices are "retrieved" not created like most vulkan concepts
        
        Queues:
            Physical device can have multiple types of queue.
            -Types are referred to as "Queue Families"
            -Queue Families can have multiple queues.
            -Example of queue family:
                - Graphics queue Family
                - Compute queue Family
                - Transfer queue Family
                - Present queue Family
            -Queue families can be often a combination of these!
            -When we enumerate Physical Device, we need to check that it has the required queue families.

        Logical device:
        - Acts as an interface to the physical device.
        - Will be referenced in a lot of vulkan functions.
        - Most vulkan objects are created on a logical device, and we use the reference to the logical device to create them.
        Creation is relatively simple:
            -Define queue families and number of queues in each family to assign the logical device to the queue families.
            -Define all device features and extensions that we want to use.
            -Define extensions the logical device will use.
            -No need to define Validation layers, deprecated in Vulkan 1.1.

        Extensions:
            -Vulkan does not know what a "Window" is.
            -Is it cross-platform, each platform has a different way of creating a window, so we need to create a surface to attach the window to the logical device.
            -Vulkan uses extensions to interact with the windowing system.
            -These extensions are so commonly used that they come pre-packaged with the Vulkan SDK.
            -Can choose required extensions manually, but SDL3 already provides them so we can just use them.
        
        SDL:
            -SDL is a library that provides a cross-platform API for creating windows and handling events.
            -It is used to create a window and attach it to the logical device.
            -It is also used to handle events such as mouse clicks, keyboard input, and window resize.
            -It is used to create a surface to attach the window to the logical device.
            -It is also used to create a Vulkan surface to attach the window to the logical device.

        Validation layers:
            -By default, validation layers are not enabled.
                -Simply crashes the application.
                -To enable validation layers, we need to define them in the logical device creation.
            -Must be enabled.
            -Each "Layer" can check different functions and different parameters.
                -VK_LAYER_LUNARG_validation is a common all-round layer
                -Layers similar to extensions, are not part of the Vulkan, must be provided by third parties.
            -Reporting of validation errors is not core of vulkan, must be provided by third parties.
            -Enable validation layers in debug builds.
            -Disable validation layers in release builds.
    */

    /* On any init failure: throw std::runtime_error so the constructor fails; main() catches it and exits with EXIT_FAILURE. Do not return — leaving Vulkan half-initialized would cause crashes later. */
    /* Tutorial order: createInstance → setupDebugMessenger → createSurface (SDL_Vulkan_*) → pickPhysicalDevice → createLogicalDevice → createSwapChain → createImageViews → createRenderPass → createGraphicsPipeline → createFramebuffers → createCommandPool → createCommandBuffers → createSyncObjects */

    this->CreateVulkanInstance();
    this->GetPhysicalDevice();
    this->CreateLogicalDevice();
}

void VulkanApp::MainLoop() {
    VulkanUtils::LogTrace("MainLoop");
    bool bQuit = static_cast<bool>(false);

    while (bQuit == false) {
        SDL_Event evt;
        while (SDL_PollEvent(&evt)) {
            switch (evt.type) {
                case SDL_EVENT_QUIT:
                    bQuit = static_cast<bool>(true);
                    break;
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    this->bFramebufferResized = static_cast<bool>(true);
                    break;
                case SDL_EVENT_WINDOW_MINIMIZED:
                    this->bWindowMinimized = static_cast<bool>(true);
                    break;
                case SDL_EVENT_WINDOW_MAXIMIZED:
                case SDL_EVENT_WINDOW_RESTORED:
                    this->bWindowMinimized = static_cast<bool>(false);
                    this->bFramebufferResized = static_cast<bool>(true);
                    break;
                case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
                    this->bFramebufferResized = static_cast<bool>(true);
                    break;
                case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
                case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
                    this->bFramebufferResized = static_cast<bool>(true);
                    break;
                default:
                    break;
            }
        }
        if (bQuit) break;

        if (this->bWindowMinimized)
            continue;

        /* Tutorial: if (bFramebufferResized) { recreateSwapChain(); bFramebufferResized = false; } then drawFrame(); in drawFrame handle VK_ERROR_OUT_OF_DATE_KHR */
        this->DrawFrame();
    }
}

void VulkanApp::Run() {
    this->MainLoop();
}

void VulkanApp::Cleanup() {
    /* Vulkan first (reverse of init): sync → command pool → framebuffers → pipeline → render pass → image views → swapchain → device → surface → debug → instance. */
    if (this->stMainDevice.pLogicalDevice != nullptr) {
        vkDestroyDevice(this->stMainDevice.pLogicalDevice, nullptr);
        this->stMainDevice.pLogicalDevice = static_cast<VkDevice>(nullptr);
    }
    if (this->pVulkanInstance != nullptr) {
        vkDestroyInstance(this->pVulkanInstance, nullptr);
        this->pVulkanInstance = static_cast<VkInstance>(nullptr);
    }

    if (this->pWindow != nullptr) {
        SDL_DestroyWindow(this->pWindow);
        this->pWindow = static_cast<SDL_Window*>(nullptr);
    }
    SDL_Quit();
}

void VulkanApp::DrawFrame() {
    /* Tutorial: vkAcquireNextImageKHR → record command buffer → submit → vkQueuePresentKHR; on VK_ERROR_OUT_OF_DATE_KHR call recreateSwapChain() and retry */
}

void VulkanApp::CreateVulkanInstance() {
    VulkanUtils::LogTrace("CreateVulkanInstance");

    /* App/engine identity and requested API version for the instance. */
    VkApplicationInfo stApplicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,     /* Required for every Vulkan struct. */
        .pNext = nullptr,                                 /* Chain optional extension structs. */
        .pApplicationName = "Custom Vulkan App",           /* Shown in tools/debuggers. */
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Custom Vulkan Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_4,                 /* Request 1.4; loader/driver may report lower. */
    };

    /* Extension names we must enable so SDL can create a Vulkan surface (platform-specific). */
    uint32_t lExtensionCount = static_cast<uint32_t>(0);
    const char* const* pExtensionNames = SDL_Vulkan_GetInstanceExtensions(&lExtensionCount);
    if (pExtensionNames == nullptr) {
        VulkanUtils::LogErr("SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
    }
    if (lExtensionCount == static_cast<uint32_t>(0)) {
        VulkanUtils::LogErr("No Vulkan instance extensions from SDL");
        throw std::runtime_error("No Vulkan instance extensions from SDL");
    }

    /* Ensure every requested extension is available (no instance needed for enumeration). */
    this->CheckInstanceExtensionsAvailable(pExtensionNames, lExtensionCount);

    /* TODO: enable validation layers and VK_EXT_DEBUG_UTILS in debug builds. */

    /* Create instance with SDL-required extensions. */
    VkInstanceCreateInfo stCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,    /* Required for every Vulkan struct. */
        .pNext = nullptr,
        .flags = static_cast<VkInstanceCreateFlags>(0),
        .pApplicationInfo = &stApplicationInfo,             /* App/API version from above. */
        .enabledLayerCount = static_cast<uint32_t>(0),      /* TODO: validation layers in debug. */
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = lExtensionCount,           /* Count from SDL_Vulkan_GetInstanceExtensions. */
        .ppEnabledExtensionNames = pExtensionNames,         /* Names from SDL (surface, platform). */
    };

    const auto result = vkCreateInstance(&stCreateInfo, nullptr, &this->pVulkanInstance);
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateInstance failed: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

void VulkanApp::CheckInstanceExtensionsAvailable(const char* const* pExtensionNames, uint32_t lExtensionCount) {
    VulkanUtils::LogTrace("CheckInstanceExtensionsAvailable");

    uint32_t lAvailableCount = static_cast<uint32_t>(0);
    vkEnumerateInstanceExtensionProperties(nullptr, &lAvailableCount, nullptr);
    std::vector<VkExtensionProperties> vecAvailableExtensions(lAvailableCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &lAvailableCount, vecAvailableExtensions.data());

    for (uint32_t i = static_cast<uint32_t>(0); i < lExtensionCount; ++i) {
        const char* pName = pExtensionNames[i];
        bool bFound = static_cast<bool>(false);
        for (const auto& stProp : vecAvailableExtensions) {
            if (std::strcmp(pName, stProp.extensionName) == static_cast<int>(0)) {
                bFound = static_cast<bool>(true);
                break;
            }
        }
        if (bFound == false) {
            VulkanUtils::LogErr("Instance extension not available: {}", pName);
            throw std::runtime_error("Required Vulkan instance extension not available");
        }
    }
}

void VulkanApp::GetPhysicalDevice() {
    VulkanUtils::LogTrace("GetPhysicalDevice");

    uint32_t lPhysicalDeviceCount = static_cast<uint32_t>(0);
    vkEnumeratePhysicalDevices(this->pVulkanInstance, &lPhysicalDeviceCount, nullptr);
    if (lPhysicalDeviceCount == static_cast<uint32_t>(0)) {
        VulkanUtils::LogErr("No Vulkan physical devices found");
        throw std::runtime_error("No Vulkan physical devices found");
    }
    std::vector<VkPhysicalDevice> vecPhysicalDevices(lPhysicalDeviceCount);
    vkEnumeratePhysicalDevices(this->pVulkanInstance, &lPhysicalDeviceCount, vecPhysicalDevices.data());

    /*
     * We do not store all devices. We loop over vecPhysicalDevices, score each, and keep only the best.
     * stMainDevice holds exactly one physical device (the chosen one) and that device’s queue family indices.
     */
    uint32_t lBestScore = static_cast<uint32_t>(0);
    VkPhysicalDevice pBestPhysicalDevice = static_cast<VkPhysicalDevice>(nullptr);

    /* Filled by vkGetPhysicalDeviceProperties per device (name, type, limits). */
    VkPhysicalDeviceProperties stPhysicalDeviceProperties = {};
    VkPhysicalDeviceProperties stBestDeviceProperties = {};
    for (const auto& pPhysicalDevice : vecPhysicalDevices) {
        vkGetPhysicalDeviceProperties(pPhysicalDevice, &stPhysicalDeviceProperties);

        const uint32_t lScore = this->RatePhysicalDeviceSuitability(pPhysicalDevice, stPhysicalDeviceProperties);
        VulkanUtils::LogInfo("Physical device: {} - Score: {}", stPhysicalDeviceProperties.deviceName, lScore);
        if (lScore > lBestScore) {
            lBestScore = lScore;
            pBestPhysicalDevice = pPhysicalDevice;
            stBestDeviceProperties = stPhysicalDeviceProperties;
        }
    }

    if (pBestPhysicalDevice == nullptr) {
        VulkanUtils::LogErr("No suitable Vulkan physical device found");
        throw std::runtime_error("No suitable Vulkan physical device found");
    }

    /* Store only the chosen device and its queue family indices (used later for logical device creation). */
    this->stMainDevice.pPhysicalDevice = pBestPhysicalDevice;
    this->GetQueueFamilies(pBestPhysicalDevice);

    VulkanUtils::LogInfo("Best physical device: {} - Score: {}", stBestDeviceProperties.deviceName, lBestScore);
}

uint32_t VulkanApp::RatePhysicalDeviceSuitability(VkPhysicalDevice pPhysicalDevice, const VkPhysicalDeviceProperties& stProperties) {
    /* Require a graphics queue family; otherwise device is unsuitable. */
    QueueFamilyIndices stIndices = this->FindQueueFamilyIndices(pPhysicalDevice);
    if (stIndices.graphicsFamily == QUEUE_FAMILY_IGNORED) {
        return static_cast<uint32_t>(0);
    }

    uint32_t lScore = static_cast<uint32_t>(0);

    /* Prefer discrete GPU over integrated, then virtual/CPU. */
    switch (stProperties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            lScore += static_cast<uint32_t>(1000);
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            lScore += static_cast<uint32_t>(100);
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            lScore += static_cast<uint32_t>(50);
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            lScore += static_cast<uint32_t>(10);
            break;
        default:
            break;
    }

    /* Filled by vkGetPhysicalDeviceFeatures; we require geometryShader for this app. */
    VkPhysicalDeviceFeatures stDeviceFeatures = {};
    vkGetPhysicalDeviceFeatures(pPhysicalDevice, &stDeviceFeatures);
    if (stDeviceFeatures.geometryShader == VK_FALSE) {
        return static_cast<uint32_t>(0);
    }

    return lScore;
}

QueueFamilyIndices VulkanApp::FindQueueFamilyIndices(VkPhysicalDevice pPhysicalDevice) {
    /* graphicsFamily / presentFamily set to index or QUEUE_FAMILY_IGNORED if not found. */
    QueueFamilyIndices stIndices = {};

    uint32_t lQueueFamilyCount = static_cast<uint32_t>(0);
    vkGetPhysicalDeviceQueueFamilyProperties(pPhysicalDevice, &lQueueFamilyCount, nullptr);
    if (lQueueFamilyCount == static_cast<uint32_t>(0)) {
        return stIndices;
    }
    std::vector<VkQueueFamilyProperties> vecQueueFamilyProperties(lQueueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(pPhysicalDevice, &lQueueFamilyCount, vecQueueFamilyProperties.data());

    for (uint32_t i = static_cast<uint32_t>(0); i < lQueueFamilyCount; ++i) {
        const auto& stProp = vecQueueFamilyProperties[i];  /* queueFlags, queueCount, etc. */
        if ((stProp.queueFlags & VK_QUEUE_GRAPHICS_BIT) != static_cast<VkQueueFlags>(0)) {
            stIndices.graphicsFamily = i;
            break;
        }
    }
    return stIndices;
}

void VulkanApp::GetQueueFamilies(VkPhysicalDevice pPhysicalDevice) {
    VulkanUtils::LogTrace("GetQueueFamilies");
    this->stMainDevice.stQueueFamilyIndices = this->FindQueueFamilyIndices(pPhysicalDevice);
}

/*
 * CreateLogicalDevice: request one graphics queue and required features (e.g. geometryShader), create the device, then retrieve the graphics queue handle.
 * Present queue is retrieved later once we have a surface and presentFamily is set (FindQueueFamilyIndices with surface).
 */
void VulkanApp::CreateLogicalDevice() {
    VulkanUtils::LogTrace("CreateLogicalDevice");

    const uint32_t lGraphicsFamily = this->stMainDevice.stQueueFamilyIndices.graphicsFamily;
    if (lGraphicsFamily == QUEUE_FAMILY_IGNORED) {
        VulkanUtils::LogErr("Graphics queue family not found");
        throw std::runtime_error("Graphics queue family not found");
    }

    /*
     * One queue from the graphics family we found in GetPhysicalDevice.
     * pQueuePriorities: one float per queue (we have queueCount 1). Value in [0, 1]; higher = more GPU time when multiple queues compete.
     */
    const float fQueuePriority = static_cast<float>(1.0);
    VkDeviceQueueCreateInfo stQueueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = static_cast<VkDeviceQueueCreateFlags>(0),
        .queueFamilyIndex = lGraphicsFamily,
        .queueCount = static_cast<uint32_t>(1),
        .pQueuePriorities = &fQueuePriority,
    };

    VkPhysicalDeviceFeatures stDeviceFeatures = {};
    vkGetPhysicalDeviceFeatures(this->stMainDevice.pPhysicalDevice, &stDeviceFeatures);
    if (stDeviceFeatures.geometryShader == VK_FALSE) {
        VulkanUtils::LogErr("Physical device does not support geometry shaders");
        throw std::runtime_error("Physical device does not support geometry shaders");
    }

    VkDeviceCreateInfo stCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = static_cast<VkDeviceCreateFlags>(0),
        .queueCreateInfoCount = static_cast<uint32_t>(1),
        .pQueueCreateInfos = &stQueueCreateInfo,
        .enabledLayerCount = static_cast<uint32_t>(0),      /* Validation layers at instance since Vulkan 1.1. */
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(0),  /* TODO: add VK_KHR_swapchain when implementing swapchain. */
        .ppEnabledExtensionNames = nullptr,
        .pEnabledFeatures = &stDeviceFeatures,
    };

    const auto result = vkCreateDevice(
        this->stMainDevice.pPhysicalDevice,
        &stCreateInfo,
        nullptr,
        &this->stMainDevice.pLogicalDevice
    );
    if (result != VK_SUCCESS) {
        VulkanUtils::LogErr("vkCreateDevice failed: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to create logical device");
    }

    /* Retrieve the graphics queue handle (queues are created with the device; index 0 = first queue in this family). */
    constexpr uint32_t QUEUE_INDEX_FIRST = static_cast<uint32_t>(0);
    vkGetDeviceQueue(
        this->stMainDevice.pLogicalDevice,
        lGraphicsFamily,              /* Graphics queue family from FindQueueFamilyIndices. */
        QUEUE_INDEX_FIRST,                  /* Queue index. */
        &this->stMainDevice.graphicsQueue       /* Pointer to the queue. */
    );
}