#include "vulkan_utils.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
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

}
