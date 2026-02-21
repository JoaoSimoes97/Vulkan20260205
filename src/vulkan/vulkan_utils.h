#pragma once

#include <cstdint>
#include <format>
#include <iostream>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace VulkanUtils {

    /* Log levels: bitwise mask (1 bit per level); Trace/Debug/Info disabled in Release. */
    enum class LogLevel : int {
        Trace = static_cast<int>(0),
        Debug = static_cast<int>(1),
        Info  = static_cast<int>(2),
        Warn  = static_cast<int>(3),
        Error = static_cast<int>(4)
    };

    /* Bit mask: 1 bit per level. Default 0 = no logs until you set in main. */
    inline constexpr uint8_t LOG_TRACE = static_cast<uint8_t>(1u << static_cast<unsigned>(0));
    inline constexpr uint8_t LOG_DEBUG = static_cast<uint8_t>(1u << static_cast<unsigned>(1));
    inline constexpr uint8_t LOG_INFO  = static_cast<uint8_t>(1u << static_cast<unsigned>(2));
    inline constexpr uint8_t LOG_WARN  = static_cast<uint8_t>(1u << static_cast<unsigned>(3));
    inline constexpr uint8_t LOG_ERROR = static_cast<uint8_t>(1u << static_cast<unsigned>(4));
    inline constexpr uint8_t LOG_ALL   = (LOG_TRACE | LOG_DEBUG | LOG_INFO | LOG_WARN | LOG_ERROR);

    inline uint8_t& LogLevelMask() {
        static uint8_t cMask = static_cast<uint8_t>(0);
        return cMask;
    }
    inline void SetLogLevelMask(uint8_t cMask) { LogLevelMask() = cMask; }
    inline uint8_t GetLogLevelMask() { return LogLevelMask(); }

    /* Fixed-width tag so all log lines align: "[Vulkan LEVEL]" = 15 chars (LEVEL padded to 5). */
    inline constexpr const char* LOG_TAG_TRACE = "[Vulkan TRACE]";
    inline constexpr const char* LOG_TAG_DEBUG = "[Vulkan DEBUG]";
    inline constexpr const char* LOG_TAG_INFO  = "[Vulkan INFO ]";
    inline constexpr const char* LOG_TAG_WARN  = "[Vulkan WARN ]";
    inline constexpr const char* LOG_TAG_ERROR = "[Vulkan ERROR]";

    /* ANSI colors for terminal (level-based). Reset after tag so message uses default. */
    inline constexpr const char* LOG_COLOR_TRACE = "\033[2m";   /* Dim */
    inline constexpr const char* LOG_COLOR_DEBUG = "\033[36m";   /* Cyan */
    inline constexpr const char* LOG_COLOR_INFO = "\033[32m";   /* Green */
    inline constexpr const char* LOG_COLOR_WARN  = "\033[33m";   /* Yellow */
    inline constexpr const char* LOG_COLOR_ERROR = "\033[31m";   /* Red */
    inline constexpr const char* LOG_COLOR_RESET = "\033[0m";

#ifdef NDEBUG
    /* Release: Trace/Debug/Info no-op; Warn and Error check mask and print. */
    template<typename... Args> inline void Log(LogLevel, std::format_string<Args...>, Args&&...) {}
    template<typename... Args> inline void LogTrace(std::format_string<Args...>, Args&&...) {}
    template<typename... Args> inline void LogDebug(std::format_string<Args...>, Args&&...) {}
    template<typename... Args> inline void LogInfo(std::format_string<Args...>, Args&&...) {}
    template<typename... Args>
    inline void LogWarn(std::format_string<Args...> fmt, Args&&... args) {
        if ((LogLevelMask() & LOG_WARN) == static_cast<uint8_t>(0)) return;
        std::cerr << std::format("{}{}{} {}", LOG_COLOR_WARN, LOG_TAG_WARN, LOG_COLOR_RESET, std::format(fmt, std::forward<Args>(args)...)) << '\n';
    }
    template<typename... Args>
    inline void LogErr(std::format_string<Args...> fmt, Args&&... args) {
        if ((LogLevelMask() & LOG_ERROR) == static_cast<uint8_t>(0)) return;
        std::cerr << std::format("{}{}{} {}", LOG_COLOR_ERROR, LOG_TAG_ERROR, LOG_COLOR_RESET, std::format(fmt, std::forward<Args>(args)...)) << '\n';
    }
#else
    /* Debug: print only if the bit for this level is set in LogLevelMask(). */
    template<typename... Args>
    inline void Log(LogLevel eLevel, std::format_string<Args...> fmt, Args&&... args) {
        if ((LogLevelMask() & (1u << static_cast<unsigned>(eLevel))) == static_cast<uint8_t>(0)) return;
        std::string sMsg = std::format(fmt, std::forward<Args>(args)...);
        const char* pTag =
            eLevel == LogLevel::Trace ? LOG_TAG_TRACE :
            eLevel == LogLevel::Debug ? LOG_TAG_DEBUG :
            eLevel == LogLevel::Info  ? LOG_TAG_INFO  :
            eLevel == LogLevel::Warn  ? LOG_TAG_WARN  : LOG_TAG_ERROR;
        const char* pColor =
            eLevel == LogLevel::Trace ? LOG_COLOR_TRACE :
            eLevel == LogLevel::Debug ? LOG_COLOR_DEBUG :
            eLevel == LogLevel::Info  ? LOG_COLOR_INFO  :
            eLevel == LogLevel::Warn  ? LOG_COLOR_WARN  : LOG_COLOR_ERROR;
        if ((eLevel == LogLevel::Error) || (eLevel == LogLevel::Warn))
            std::cerr << std::format("{}{}{} {}", pColor, pTag, LOG_COLOR_RESET, sMsg) << '\n';
        else
            std::cout << std::format("{}{}{} {}", pColor, pTag, LOG_COLOR_RESET, sMsg) << '\n';
    }
    template<typename... Args> inline void LogTrace(std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Trace, fmt, std::forward<Args>(args)...);
    }
    template<typename... Args> inline void LogDebug(std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Debug, fmt, std::forward<Args>(args)...);
    }
    template<typename... Args> inline void LogInfo(std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Info, fmt, std::forward<Args>(args)...);
    }
    template<typename... Args> inline void LogWarn(std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Warn, fmt, std::forward<Args>(args)...);
    }
    template<typename... Args> inline void LogErr(std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Error, fmt, std::forward<Args>(args)...);
    }
#endif

    /* Validation layer name for debug builds. */
    const std::vector<const char*> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
    };

#ifdef NDEBUG
    constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
    constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif

    /* Returns the directory containing the executable. Empty on failure. */
    std::string GetExecutableDirectory();

    /* Returns the project source directory (from PROJECT_SOURCE_DIR compile definition). Empty if not defined. */
    std::string GetProjectSourceDirectory();

    /* Base directory for shipped resources: exe dir if it contains shaders/, else exe parent (install/bin layout). Use for all paths when shipping. */
    std::string GetResourceBaseDir();

    /* 
     * Path for a resource relative to the project.
     * For editable resources (config/, levels/, models/): checks PROJECT_SOURCE_DIR first (dev workflow),
     * falls back to exe-relative (shipped/install scenarios).
     * For compiled artifacts (shaders/): always exe-relative.
     */
    std::string GetResourcePath(const std::string& sPath);

    /* Resolve path for reading: same as GetResourcePath (exe-relative); kept for compatibility. */
    std::string ResolveResourcePath(const std::string& sPath);

    /* Helper functions. */
    std::vector<char> ReadFile(const std::string& sFilename);
    bool CheckValidationLayerSupport();
    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& stCreateInfo);
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                          const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                          const VkAllocationCallbacks* pAllocator,
                                          VkDebugUtilsMessengerEXT* pDebugMessenger);
    void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                       VkDebugUtilsMessengerEXT debugMessenger,
                                       const VkAllocationCallbacks* pAllocator);
    VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                 VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                 void* pUserData);
}
