#include "vulkan_utils.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#endif

namespace VulkanUtils {

std::string GetExecutableDirectory() {
    std::string sPath;
#if defined(_WIN32) || defined(_WIN64)
    wchar_t vecBuf[MAX_PATH];
    DWORD lLen = GetModuleFileNameW(static_cast<HMODULE>(nullptr), vecBuf, static_cast<DWORD>(MAX_PATH));
    if (lLen == static_cast<DWORD>(0))
        return std::string();
    std::filesystem::path p(vecBuf);
    std::filesystem::path dir = p.parent_path();
    sPath = dir.string();
#elif defined(__APPLE__)
    char vecBuf[PATH_MAX];
    uint32_t lSize = sizeof(vecBuf);
    if (_NSGetExecutablePath(vecBuf, &lSize) != 0)
        return std::string();
    std::filesystem::path p = std::filesystem::canonical(vecBuf);
    std::filesystem::path dir = p.parent_path();
    sPath = dir.string();
#elif defined(__linux__)
    char vecBuf[PATH_MAX];
    ssize_t zLen = readlink("/proc/self/exe", vecBuf, sizeof(vecBuf) - static_cast<size_t>(1));
    if (zLen <= static_cast<ssize_t>(0))
        return std::string();
    vecBuf[zLen] = '\0';
    std::filesystem::path p(vecBuf);
    std::filesystem::path dir = p.parent_path();
    sPath = dir.string();
#else
    (void)0;
    return std::string();
#endif
    return sPath;
}

std::string GetProjectSourceDirectory() {
#if defined(PROJECT_SOURCE_DIR)
    return PROJECT_SOURCE_DIR;
#else
    return std::string();
#endif
}

/* Check if a path is for editable resources (config, levels, models) vs compiled artifacts (shaders). */
static bool IsEditableResourcePath(const std::string& sPath) {
    if (sPath.rfind("config/", 0) == 0 || sPath.rfind("config\\", 0) == 0) return true;
    if (sPath.rfind("levels/", 0) == 0 || sPath.rfind("levels\\", 0) == 0) return true;
    if (sPath.rfind("models/", 0) == 0 || sPath.rfind("models\\", 0) == 0) return true;
    return false;
}

std::string GetResourceBaseDir() {
    std::string sExeDir = GetExecutableDirectory();
    if (sExeDir.empty() == true)
        return std::string();
    std::filesystem::path exeDir(sExeDir);
    std::filesystem::path shadersDir = exeDir / "shaders";
    if (std::filesystem::exists(shadersDir) == true)
        return exeDir.string();
    std::filesystem::path parentShaders = exeDir.parent_path() / "shaders";
    if (std::filesystem::exists(parentShaders) == true)
        return exeDir.parent_path().string();
    return exeDir.string();
}

std::string GetResourcePath(const std::string& sPath) {
    // For editable resources (config, levels, models): prefer PROJECT_SOURCE_DIR.
    // This allows editing files in ONE place (project root) during development.
    // Shaders stay exe-relative (they're compiled artifacts).
    // Falls back to exe-relative for distribution/install scenarios.
    if (IsEditableResourcePath(sPath)) {
        std::string sSrcDir = GetProjectSourceDirectory();
        if (!sSrcDir.empty()) {
            std::filesystem::path srcPath = std::filesystem::path(sSrcDir) / sPath;
            if (std::filesystem::exists(srcPath))
                return srcPath.lexically_normal().string();
        }
    }
    // Fallback: exe-relative (shaders, or install/distribution scenario)
    std::string sBase = GetResourceBaseDir();
    if (sBase.empty() == true)
        return sPath;
    std::filesystem::path full = std::filesystem::path(sBase) / std::filesystem::path(sPath);
    return full.lexically_normal().string();
}

std::string ResolveResourcePath(const std::string& sPath) {
    return GetResourcePath(sPath);
}

bool CheckValidationLayerSupport() {
    uint32_t lLayerCount = static_cast<uint32_t>(0);
    vkEnumerateInstanceLayerProperties(&lLayerCount, nullptr);
    std::vector<VkLayerProperties> vecAvailableLayers(lLayerCount);
    vkEnumerateInstanceLayerProperties(&lLayerCount, vecAvailableLayers.data());

    for (const char* pLayerName : VALIDATION_LAYERS) {
        bool bLayerFound = static_cast<bool>(false);
        for (const auto& stLayerProperties : vecAvailableLayers) {
            if (std::strcmp(pLayerName, stLayerProperties.layerName) == static_cast<int>(0)) {
                bLayerFound = static_cast<bool>(true);
                break;
            }
        }
        if (bLayerFound == false) {
            return false;
        }
    }
    return true;
}

void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& stCreateInfo) {
    stCreateInfo = {};
    stCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    stCreateInfo.messageSeverity =
        static_cast<VkDebugUtilsMessageSeverityFlagsEXT>(
            static_cast<uint32_t>(VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) |
            static_cast<uint32_t>(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) |
            static_cast<uint32_t>(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT));
    stCreateInfo.messageType =
        static_cast<VkDebugUtilsMessageTypeFlagsEXT>(
            static_cast<uint32_t>(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) |
            static_cast<uint32_t>(VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) |
            static_cast<uint32_t>(VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT));
    stCreateInfo.pfnUserCallback = DebugCallback;
    stCreateInfo.pUserData = nullptr;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                      const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator,
                                      VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto pfnCreate = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (pfnCreate != nullptr) {
        return pfnCreate(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks* pAllocator) {
    auto pfnDestroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (pfnDestroy != nullptr) {
        pfnDestroy(instance, debugMessenger, pAllocator);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                             VkDebugUtilsMessageTypeFlagsEXT messageType,
                                             const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                             void* pUserData) {
    (void)messageType;
    (void)pUserData;
    if ((pCallbackData == nullptr) || (pCallbackData->pMessage == nullptr)) {
        return VK_FALSE;
    }
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        LogErr("validation: {}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LogWarn("validation: {}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        LogInfo("validation: {}", pCallbackData->pMessage);
    } else {
        LogDebug("validation: {}", pCallbackData->pMessage);
    }
    return VK_FALSE;
}

uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t lTypeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = static_cast<uint32_t>(0); i < memProps.memoryTypeCount; ++i) {
        if (((lTypeFilter & (static_cast<uint32_t>(1) << i)) != static_cast<uint32_t>(0)) &&
            ((memProps.memoryTypes[i].propertyFlags & properties) == properties)) {
            return i;
        }
    }
    throw std::runtime_error("VulkanUtils::FindMemoryType: no suitable memory type");
}

VkResult CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                      VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps,
                      VkBuffer* outBuffer, VkDeviceMemory* outMemory) {
    VkBufferCreateInfo bufInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = static_cast<VkBufferCreateFlags>(0),
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };
    VkResult r = vkCreateBuffer(device, &bufInfo, nullptr, outBuffer);
    if (r != VK_SUCCESS) return r;
    
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, *outBuffer, &memReqs);
    
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = FindMemoryType(physicalDevice, memReqs.memoryTypeBits, memProps),
    };
    r = vkAllocateMemory(device, &allocInfo, nullptr, outMemory);
    if (r != VK_SUCCESS) {
        vkDestroyBuffer(device, *outBuffer, nullptr);
        *outBuffer = VK_NULL_HANDLE;
        return r;
    }
    
    vkBindBufferMemory(device, *outBuffer, *outMemory, 0);
    return VK_SUCCESS;
}

VkCommandBuffer BeginSingleTimeCommands(VkDevice device, VkCommandPool pool) {
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &allocInfo, &cmd) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        return VK_NULL_HANDLE;
    }
    return cmd;
}

void EndSingleTimeCommands(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr,
    };
    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, pool, 1, &cmd);
}

}
