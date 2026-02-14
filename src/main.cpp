/*
 * Entry point. Configures logging (Debug = all levels, Release = Warn+Error), runs VulkanApp, exits.
 */
#include "vulkan_app.h"
#include "vulkan_utils.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
#ifdef NDEBUG
    VulkanUtils::SetLogLevelMask(VulkanUtils::LOG_ERROR | VulkanUtils::LOG_WARN);
#else
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
