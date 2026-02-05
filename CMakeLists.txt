cmake_minimum_required(VERSION 3.20)
project(VulkanApp VERSION 1.0.0 LANGUAGES CXX)

# So clangd (and other tools) can find includes and definitions.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug vs Release: default to Debug so logging and validation are on unless you choose Release.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type: Debug (logging, validation) or Release (optimized, no logging)" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE} (Release defines NDEBUG, so logging is no-op)")

# Platform-specific settings
if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

# Find packages: SDL3 (try system first, else FetchContent), Vulkan
set(SDL3_MIN_VERSION "3.0.0")
find_package(SDL3 ${SDL3_MIN_VERSION} QUIET)
if(NOT SDL3_FOUND)
    message(STATUS "SDL3 not found, fetching via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
        GIT_TAG "main"
        GIT_SHALLOW TRUE
    )
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3)
endif()
find_package(Vulkan REQUIRED)

# Vulkan: Linux/Windows use native drivers (no MoltenVK). macOS uses Vulkan SDK which uses MoltenVK (Vulkan → Metal).
# Same Vulkan code everywhere; zero cost on Linux/Windows.
if(APPLE)
    message(STATUS "Apple: Vulkan via MoltenVK (Vulkan SDK on macOS)")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${Vulkan_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/main.cpp
    src/vulkan_app.cpp
    src/vulkan_utils.cpp
)

# Header files
set(HEADERS
    include/vulkan_app.h
    include/vulkan_utils.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Set include directories for the target
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    SDL3::SDL3
    ${Vulkan_LIBRARIES}
)

# Platform-specific linking
if(WIN32)
    target_link_libraries(${PROJECT_NAME} ${CMAKE_DL_LIBS})
    # Copy SDL3.dll next to the executable (needed when using FetchContent or vcpkg)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "Copying SDL3.dll to output directory"
    )
elseif(APPLE)
    # MoltenVK (Vulkan SDK on macOS) needs these frameworks
    target_link_libraries(${PROJECT_NAME}
        "-framework Cocoa"
        "-framework Metal"
        "-framework Foundation"
        "-framework QuartzCore"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework IOSurface"
        "-framework CoreGraphics"
        "-framework AppKit"
    )
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} ${CMAKE_DL_LIBS} pthread)
endif()

# Auto-compile shaders
find_program(GLSLC glslc)
find_program(GLSLANGVALIDATOR glslangValidator)

if(GLSLC)
    message(STATUS "Found glslc, will auto-compile shaders")
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/shaders/vert.spv
        COMMAND ${GLSLC} ${CMAKE_SOURCE_DIR}/shaders/vert.vert -o ${CMAKE_BINARY_DIR}/shaders/vert.spv
        DEPENDS ${CMAKE_SOURCE_DIR}/shaders/vert.vert
        COMMENT "Compiling vertex shader"
    )
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/shaders/frag.spv
        COMMAND ${GLSLC} ${CMAKE_SOURCE_DIR}/shaders/frag.frag -o ${CMAKE_BINARY_DIR}/shaders/frag.spv
        DEPENDS ${CMAKE_SOURCE_DIR}/shaders/frag.frag
        COMMENT "Compiling fragment shader"
    )
elseif(GLSLANGVALIDATOR)
    message(STATUS "Found glslangValidator, will auto-compile shaders")
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/shaders/vert.spv
        COMMAND ${GLSLANGVALIDATOR} -V ${CMAKE_SOURCE_DIR}/shaders/vert.vert -o ${CMAKE_BINARY_DIR}/shaders/vert.spv
        DEPENDS ${CMAKE_SOURCE_DIR}/shaders/vert.vert
        COMMENT "Compiling vertex shader"
    )
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/shaders/frag.spv
        COMMAND ${GLSLANGVALIDATOR} -V ${CMAKE_SOURCE_DIR}/shaders/frag.frag -o ${CMAKE_BINARY_DIR}/shaders/frag.spv
        DEPENDS ${CMAKE_SOURCE_DIR}/shaders/frag.frag
        COMMENT "Compiling fragment shader"
    )
else()
    message(WARNING "No shader compiler found (glslc or glslangValidator). Shaders must be compiled manually.")
    # Copy shaders directory anyway
    file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})
endif()

# Create shaders directory in build folder
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)

# Add shader compilation as a custom target
add_custom_target(compile_shaders ALL
    DEPENDS ${CMAKE_BINARY_DIR}/shaders/vert.spv ${CMAKE_BINARY_DIR}/shaders/frag.spv
)

# Local install/bundle: output folder (e.g. install/Debug, install/Release)
set(INSTALL_OUTPUT_DIR "install" CACHE STRING "Install output folder (e.g. install/Debug, install/Release)")
set(INSTALL_DIR ${CMAKE_SOURCE_DIR}/${INSTALL_OUTPUT_DIR})

add_custom_target(install_local
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${INSTALL_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${INSTALL_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${INSTALL_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${INSTALL_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${INSTALL_DIR}/bin/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/shaders ${INSTALL_DIR}/shaders
    DEPENDS ${PROJECT_NAME} compile_shaders
)

# Symlink compile_commands.json to project root so clangd finds it (go-to-definition, etc.).
file(RELATIVE_PATH REL_COMPILE_COMMANDS ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/compile_commands.json)
execute_process(COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/compile_commands.json)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${REL_COMPILE_COMMANDS} ${CMAKE_SOURCE_DIR}/compile_commands.json
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
