#include "vulkan_app.h"
#include "vulkan_utils.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

/*
 * Log levels: Trace (LogTrace), Debug (LogDebug), Info (LogInfo), Warn (LogWarn), Error (LogErr).
 * Set mask in main; Release disables Trace/Debug/Info; Warn and Error remain for production.
 */

int main() {
#ifdef NDEBUG
    /* Release: Error and Warn so production failures are visible. */
    VulkanUtils::SetLogLevelMask(VulkanUtils::LOG_ERROR | VulkanUtils::LOG_WARN);
#else
    /* Debug: enable all log levels (set in main; otherwise none). */
    VulkanUtils::SetLogLevelMask(VulkanUtils::LOG_ALL);
#endif

    try {
        VulkanApp app;
        app.Run();
    } catch (const std::exception& e) {
        VulkanUtils::LogErr("Exception: {}", e.what());
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    VulkanUtils::LogInfo("Exiting normally");
    return EXIT_SUCCESS;
}
