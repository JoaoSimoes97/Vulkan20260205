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

# Find packages: SDL3, Vulkan (must be installed via setup / package manager; no download during build)
set(SDL3_MIN_VERSION "3.0.0")
find_package(SDL3 ${SDL3_MIN_VERSION} REQUIRED)
find_package(Vulkan REQUIRED)

# JSON config: nlohmann/json (required; must be installed — see README for per-OS package names)
find_package(nlohmann_json 3.11 REQUIRED)

# Math library: GLM (header-only, for graphics math like matrices, vectors, and pi constant)
find_package(glm REQUIRED)

# stb (header-only): must be provided by setup in deps/stb — no download during build
set(DEPS_STB_DIR ${CMAKE_SOURCE_DIR}/deps/stb CACHE PATH "Path to stb (populated by setup)")
if(NOT EXISTS "${DEPS_STB_DIR}/stb_image.h")
    message(FATAL_ERROR "stb not found at ${DEPS_STB_DIR}. Run the project setup script first (e.g. ./setup.sh or scripts/linux/setup_linux.sh).")
endif()
message(STATUS "Using stb from deps: ${DEPS_STB_DIR}")

# TinyGLTF: prefer system package; else use deps/tinygltf (populated by setup)
find_package(TinyGLTF QUIET)
if(NOT TinyGLTF_FOUND)
    set(DEPS_TINYGLTF_DIR ${CMAKE_SOURCE_DIR}/deps/tinygltf CACHE PATH "Path to TinyGLTF (populated by setup)")
    if(EXISTS "${DEPS_TINYGLTF_DIR}/tiny_gltf.h")
        message(STATUS "Using TinyGLTF from deps: ${DEPS_TINYGLTF_DIR}")
        set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
        set(TINYGLTF_BUILD_GL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(TINYGLTF_BUILD_VALIDATOR_EXAMPLE OFF CACHE BOOL "" FORCE)
        set(TINYGLTF_BUILD_BUILDER_EXAMPLE OFF CACHE BOOL "" FORCE)
        set(TINYGLTF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(TINYGLTF_HEADER_ONLY OFF CACHE BOOL "" FORCE)
        set(TINYGLTF_INSTALL OFF CACHE BOOL "" FORCE)
        add_subdirectory(${DEPS_TINYGLTF_DIR} ${CMAKE_BINARY_DIR}/deps/tinygltf)
        target_compile_definitions(tinygltf PRIVATE TINYGLTF_NO_STB_IMAGE TINYGLTF_NO_STB_IMAGE_WRITE)
    else()
        message(FATAL_ERROR "TinyGLTF not found (no system package and no deps/tinygltf). Run the project setup script first (e.g. ./setup.sh or scripts/linux/setup_linux.sh).")
    endif()
endif()

# Vulkan: Linux/Windows use native drivers (no MoltenVK). macOS uses Vulkan SDK which uses MoltenVK (Vulkan → Metal).
# Same Vulkan code everywhere; zero cost on Linux/Windows.
if(APPLE)
    message(STATUS "Apple: Vulkan via MoltenVK (Vulkan SDK on macOS)")
endif()

# Include directories: one per module (Option B — header + source in same folder)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/app)
include_directories(${CMAKE_SOURCE_DIR}/src/config)
include_directories(${CMAKE_SOURCE_DIR}/src/core)
include_directories(${CMAKE_SOURCE_DIR}/src/managers)
include_directories(${CMAKE_SOURCE_DIR}/src/thread)
include_directories(${CMAKE_SOURCE_DIR}/src/window)
include_directories(${CMAKE_SOURCE_DIR}/src/scene)
include_directories(${CMAKE_SOURCE_DIR}/src/camera)
include_directories(${CMAKE_SOURCE_DIR}/src/render)
include_directories(${CMAKE_SOURCE_DIR}/src/loaders)
include_directories(${CMAKE_SOURCE_DIR}/src/vulkan)
include_directories(${Vulkan_INCLUDE_DIRS})

# Source files (Option B: one subfolder per module)
set(SOURCES
    src/main.cpp
    src/app/vulkan_app.cpp
    src/config/vulkan_config.cpp
    src/config/config_loader.cpp
    src/thread/job_queue.cpp
    src/thread/resource_manager_thread.cpp
    src/window/window.cpp
    src/camera/camera.cpp
    src/vulkan/vulkan_utils.cpp
    src/vulkan/vulkan_instance.cpp
    src/vulkan/vulkan_device.cpp
    src/vulkan/vulkan_swapchain.cpp
    src/vulkan/vulkan_render_pass.cpp
    src/vulkan/vulkan_depth_image.cpp
    src/vulkan/vulkan_pipeline.cpp
    src/vulkan/vulkan_framebuffers.cpp
    src/vulkan/vulkan_command_buffers.cpp
    src/vulkan/vulkan_sync.cpp
    src/vulkan/vulkan_shader_manager.cpp
    src/managers/descriptor_set_layout_manager.cpp
    src/managers/descriptor_pool_manager.cpp
    src/managers/pipeline_manager.cpp
    src/managers/material_manager.cpp
    src/managers/mesh_manager.cpp
    src/managers/scene_manager.cpp
    src/managers/texture_manager.cpp
    src/managers/resource_cleanup_manager.cpp
    src/loaders/gltf_loader.cpp
    src/loaders/gltf_mesh_utils.cpp
    src/loaders/procedural_mesh_factory.cpp
    src/render/render_list_builder.cpp
    src/core/light_manager.cpp
    src/core/light_debug_renderer.cpp
    src/core/scene_new.cpp
)

# Header files (same layout)
set(HEADERS
    src/app/vulkan_app.h
    src/config/vulkan_config.h
    src/config/config_loader.h
    src/managers/managers.h
    src/managers/descriptor_set_layout_manager.h
    src/managers/descriptor_pool_manager.h
    src/managers/pipeline_manager.h
    src/managers/material_manager.h
    src/managers/mesh_manager.h
    src/managers/scene_manager.h
    src/managers/texture_manager.h
    src/managers/resource_cleanup_manager.h
    src/loaders/gltf_loader.h
    src/loaders/procedural_mesh_factory.h
    src/scene/scene.h
    src/render/render_list_builder.h
    src/thread/job_queue.h
    src/window/window.h
    src/scene/object.h
    src/vulkan/vulkan_types.h
    src/vulkan/vulkan_utils.h
    src/vulkan/vulkan_instance.h
    src/vulkan/vulkan_device.h
    src/vulkan/vulkan_swapchain.h
    src/vulkan/vulkan_render_pass.h
    src/vulkan/vulkan_depth_image.h
    src/vulkan/vulkan_pipeline.h
    src/vulkan/vulkan_framebuffers.h
    src/vulkan/vulkan_command_buffers.h
    src/vulkan/vulkan_sync.h
    src/vulkan/vulkan_shader_manager.h
    src/core/camera_component.h
    src/core/component.h
    src/core/core.h
    src/core/gameobject.h
    src/core/light_component.h
    src/core/light_debug_renderer.h
    src/core/light_manager.h
    src/core/physics_component.h
    src/core/renderer_component.h
    src/core/scene_new.h
    src/core/script_component.h
    src/core/transform.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Set include directories for the target (stb from deps/)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/app
    ${CMAKE_SOURCE_DIR}/src/config
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/loaders
    ${CMAKE_SOURCE_DIR}/src/managers
    ${CMAKE_SOURCE_DIR}/src/thread
    ${CMAKE_SOURCE_DIR}/src/window
    ${CMAKE_SOURCE_DIR}/src/scene
    ${CMAKE_SOURCE_DIR}/src/render
    ${CMAKE_SOURCE_DIR}/src/vulkan
    ${DEPS_STB_DIR}
    ${Vulkan_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    SDL3::SDL3
    ${Vulkan_LIBRARIES}
    nlohmann_json::nlohmann_json
    glm::glm
)
if(TinyGLTF_FOUND)
    target_link_libraries(${PROJECT_NAME} TinyGLTF::tinygltf)
else()
    target_link_libraries(${PROJECT_NAME} tinygltf)
    # Match tinygltf: no built-in stb (we use our own in texture_manager); avoids duplicate symbols and undefined refs.
    target_compile_definitions(${PROJECT_NAME} PRIVATE TINYGLTF_NO_STB_IMAGE TINYGLTF_NO_STB_IMAGE_WRITE)
endif()

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

# Shaders: source in shaders/source/, compiled output in build/shaders/
set(SHADERS_SOURCE_DIR ${CMAKE_SOURCE_DIR}/shaders/source)
set(SHADERS_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)

find_program(GLSLC glslc)
find_program(GLSLANGVALIDATOR glslangValidator)

if(GLSLC)
    message(STATUS "Found glslc, will auto-compile shaders (source: shaders/source/ -> build/shaders/)")
    add_custom_command(
        OUTPUT ${SHADERS_OUTPUT_DIR}/vert.spv
        COMMAND ${GLSLC} ${SHADERS_SOURCE_DIR}/vert.vert -o ${SHADERS_OUTPUT_DIR}/vert.spv
        DEPENDS ${SHADERS_SOURCE_DIR}/vert.vert
        COMMENT "Compiling vertex shader"
    )
    add_custom_command(
        OUTPUT ${SHADERS_OUTPUT_DIR}/frag.spv
        COMMAND ${GLSLC} ${SHADERS_SOURCE_DIR}/frag.frag -o ${SHADERS_OUTPUT_DIR}/frag.spv
        DEPENDS ${SHADERS_SOURCE_DIR}/frag.frag
        COMMENT "Compiling fragment shader"
    )
    add_custom_command(
        OUTPUT ${SHADERS_OUTPUT_DIR}/frag_alt.spv
        COMMAND ${GLSLC} ${SHADERS_SOURCE_DIR}/frag_alt.frag -o ${SHADERS_OUTPUT_DIR}/frag_alt.spv
        DEPENDS ${SHADERS_SOURCE_DIR}/frag_alt.frag
        COMMENT "Compiling alt fragment shader"
    )
    add_custom_command(
        OUTPUT ${SHADERS_OUTPUT_DIR}/frag_untextured.spv
        COMMAND ${GLSLC} ${SHADERS_SOURCE_DIR}/frag_untextured.frag -o ${SHADERS_OUTPUT_DIR}/frag_untextured.spv
        DEPENDS ${SHADERS_SOURCE_DIR}/frag_untextured.frag
        COMMENT "Compiling untextured fragment shader"
    )
    add_custom_command(
        OUTPUT ${SHADERS_OUTPUT_DIR}/debug_line.vert.spv
        COMMAND ${GLSLC} ${SHADERS_SOURCE_DIR}/debug_line.vert -o ${SHADERS_OUTPUT_DIR}/debug_line.vert.spv
        DEPENDS ${SHADERS_SOURCE_DIR}/debug_line.vert
        COMMENT "Compiling debug line vertex shader"
    )
    add_custom_command(
        OUTPUT ${SHADERS_OUTPUT_DIR}/debug_line.frag.spv
        COMMAND ${GLSLC} ${SHADERS_SOURCE_DIR}/debug_line.frag -o ${SHADERS_OUTPUT_DIR}/debug_line.frag.spv
        DEPENDS ${SHADERS_SOURCE_DIR}/debug_line.frag
        COMMENT "Compiling debug line fragment shader"
    )
elseif(GLSLANGVALIDATOR)
    message(STATUS "Found glslangValidator, will auto-compile shaders (source: shaders/source/ -> build/shaders/)")
    add_custom_command(
        OUTPUT ${SHADERS_OUTPUT_DIR}/vert.spv
        COMMAND ${GLSLANGVALIDATOR} -V ${SHADERS_SOURCE_DIR}/vert.vert -o ${SHADERS_OUTPUT_DIR}/vert.spv
        DEPENDS ${SHADERS_SOURCE_DIR}/vert.vert
        COMMENT "Compiling vertex shader"
    )
    add_custom_command(
        OUTPUT ${SHADERS_OUTPUT_DIR}/frag.spv
        COMMAND ${GLSLANGVALIDATOR} -V ${SHADERS_SOURCE_DIR}/frag.frag -o ${SHADERS_OUTPUT_DIR}/frag.spv
        DEPENDS ${SHADERS_SOURCE_DIR}/frag.frag
        COMMENT "Compiling fragment shader"
    )
    add_custom_command(
        OUTPUT ${SHADERS_OUTPUT_DIR}/frag_alt.spv
        COMMAND ${GLSLANGVALIDATOR} -V ${SHADERS_SOURCE_DIR}/frag_alt.frag -o ${SHADERS_OUTPUT_DIR}/frag_alt.spv
        DEPENDS ${SHADERS_SOURCE_DIR}/frag_alt.frag
        COMMENT "Compiling alt fragment shader"
    )
    add_custom_command(
        OUTPUT ${SHADERS_OUTPUT_DIR}/frag_untextured.spv
        COMMAND ${GLSLANGVALIDATOR} -V ${SHADERS_SOURCE_DIR}/frag_untextured.frag -o ${SHADERS_OUTPUT_DIR}/frag_untextured.spv
        DEPENDS ${SHADERS_SOURCE_DIR}/frag_untextured.frag
        COMMENT "Compiling untextured fragment shader"
    )
    add_custom_command(
        OUTPUT ${SHADERS_OUTPUT_DIR}/debug_line.vert.spv
        COMMAND ${GLSLANGVALIDATOR} -V ${SHADERS_SOURCE_DIR}/debug_line.vert -o ${SHADERS_OUTPUT_DIR}/debug_line.vert.spv
        DEPENDS ${SHADERS_SOURCE_DIR}/debug_line.vert
        COMMENT "Compiling debug line vertex shader"
    )
    add_custom_command(
        OUTPUT ${SHADERS_OUTPUT_DIR}/debug_line.frag.spv
        COMMAND ${GLSLANGVALIDATOR} -V ${SHADERS_SOURCE_DIR}/debug_line.frag -o ${SHADERS_OUTPUT_DIR}/debug_line.frag.spv
        DEPENDS ${SHADERS_SOURCE_DIR}/debug_line.frag
        COMMENT "Compiling debug line fragment shader"
    )
else()
    message(WARNING "No shader compiler found (glslc or glslangValidator). Shaders must be compiled manually.")
    file(MAKE_DIRECTORY ${SHADERS_OUTPUT_DIR})
endif()

file(MAKE_DIRECTORY ${SHADERS_OUTPUT_DIR})

add_custom_target(compile_shaders ALL
    DEPENDS ${SHADERS_OUTPUT_DIR}/vert.spv ${SHADERS_OUTPUT_DIR}/frag.spv ${SHADERS_OUTPUT_DIR}/frag_alt.spv ${SHADERS_OUTPUT_DIR}/frag_untextured.spv ${SHADERS_OUTPUT_DIR}/debug_line.vert.spv ${SHADERS_OUTPUT_DIR}/debug_line.frag.spv
)

# Ensure shaders are built before the app, and copy shaders + config + levels + models next to the exe so the app finds them regardless of cwd.
add_dependencies(${PROJECT_NAME} compile_shaders)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/config
    COMMAND ${CMAKE_COMMAND} -E copy ${SHADERS_OUTPUT_DIR}/vert.spv $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/
    COMMAND ${CMAKE_COMMAND} -E copy ${SHADERS_OUTPUT_DIR}/frag.spv $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/
    COMMAND ${CMAKE_COMMAND} -E copy ${SHADERS_OUTPUT_DIR}/frag_alt.spv $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/
    COMMAND ${CMAKE_COMMAND} -E copy ${SHADERS_OUTPUT_DIR}/frag_untextured.spv $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/
    COMMAND ${CMAKE_COMMAND} -E copy ${SHADERS_OUTPUT_DIR}/debug_line.vert.spv $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/
    COMMAND ${CMAKE_COMMAND} -E copy ${SHADERS_OUTPUT_DIR}/debug_line.frag.spv $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/config $<TARGET_FILE_DIR:${PROJECT_NAME}>/config
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/levels $<TARGET_FILE_DIR:${PROJECT_NAME}>/levels
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/models $<TARGET_FILE_DIR:${PROJECT_NAME}>/models
    COMMENT "Copying shaders, config, levels, and models next to executable"
)

# Local install/bundle: output folder (e.g. install/Debug, install/Release)
set(INSTALL_OUTPUT_DIR "install" CACHE STRING "Install output folder (e.g. install/Debug, install/Release)")
set(INSTALL_DIR ${CMAKE_SOURCE_DIR}/${INSTALL_OUTPUT_DIR})

add_custom_target(install_local
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${INSTALL_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${INSTALL_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${INSTALL_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${INSTALL_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E make_directory ${INSTALL_DIR}/config
    COMMAND ${CMAKE_COMMAND} -E make_directory ${INSTALL_DIR}/levels
    COMMAND ${CMAKE_COMMAND} -E make_directory ${INSTALL_DIR}/models
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${INSTALL_DIR}/bin/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADERS_OUTPUT_DIR} ${INSTALL_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/config ${INSTALL_DIR}/config
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/levels ${INSTALL_DIR}/levels
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/models ${INSTALL_DIR}/models
    DEPENDS ${PROJECT_NAME} compile_shaders
)

# Symlink compile_commands.json to project root so clangd finds it (go-to-definition, etc.).
file(RELATIVE_PATH REL_COMPILE_COMMANDS ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/compile_commands.json)
execute_process(COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/compile_commands.json)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${REL_COMPILE_COMMANDS} ${CMAKE_SOURCE_DIR}/compile_commands.json
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
