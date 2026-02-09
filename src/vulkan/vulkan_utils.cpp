#include "vulkan_utils.h"
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

}
