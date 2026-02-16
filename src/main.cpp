/*
 * Entry point. Configures logging (Debug = all levels, Release = Warn+Error), runs VulkanApp, exits.
 */
#include "vulkan_app.h"
#include "vulkan_utils.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
#ifdef NDEBUG
    VulkanUtils::SetLogLevelMask(VulkanUtils::LOG_ERROR | VulkanUtils::LOG_WARN);
#else
    VulkanUtils::SetLogLevelMask(VulkanUtils::LOG_ALL);
#endif

    // Require level path as command-line argument
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <level_path>\n";
        std::cerr << "\nExamples:\n";
        std::cerr << "  " << argv[0] << " levels/default/level.json\n";
        std::cerr << "  " << argv[0] << " levels/demo/level.json\n";
        return EXIT_FAILURE;
    }

    std::string levelPath = argv[1];

    try {
        VulkanConfig config;
        config.sLevelPath = levelPath;
        VulkanApp app(config);
        app.Run();
    } catch (const std::exception& e) {
        VulkanUtils::LogErr("Exception: {}", e.what());
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    VulkanUtils::LogInfo("Exiting normally");
    return EXIT_SUCCESS;
}
