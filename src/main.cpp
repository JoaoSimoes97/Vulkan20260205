/*
 * Entry point. Configures logging (Debug = all levels, Release = Warn+Error), runs VulkanApp, exits.
 */
#include "vulkan_app.h"
#include "vulkan_utils.h"
#include "config/config_loader.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

static const char* CONFIG_PATH_USER    = "config/config.json";
static const char* CONFIG_PATH_DEFAULT = "config/default.json";

int main(int argc, char** argv) {
#ifdef NDEBUG
    VulkanUtils::SetLogLevelMask(VulkanUtils::LOG_ERROR | VulkanUtils::LOG_WARN);
#else
    VulkanUtils::SetLogLevelMask(VulkanUtils::LOG_ALL);
#endif

    // Level path is optional - File menu (Editor) or Main Menu (Release) handles selection
    std::string levelPath;
    if (argc >= 2) {
        levelPath = argv[1];
        VulkanUtils::LogInfo("Level path from command line: {}", levelPath);
    } else {
        VulkanUtils::LogInfo("No level path provided - use File menu to load levels");
    }

    try {
        VulkanConfig config = LoadConfigFromFileOrCreate(CONFIG_PATH_USER, CONFIG_PATH_DEFAULT);
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
