# iOS: build and run

Same Vulkan code via **MoltenVK** (Vulkan → Metal). Build for iOS with Xcode (or CMake for iOS) and a minimal app that provides a Metal drawable and runs the Vulkan loop.

## What to install right now

1. **Mac** with **Xcode** (required for iOS builds)
   - [Xcode](https://developer.apple.com/xcode/)

2. **MoltenVK for iOS**
   - Vulkan SDK for macOS includes MoltenVK; for iOS you need the MoltenVK iOS framework (e.g. from [MoltenVK](https://github.com/KhronosGroup/MoltenVK) or Vulkan SDK when targeting iOS).

3. **CMake for iOS** (optional)
   - CMake can target iOS with `-DCMAKE_SYSTEM_NAME=iOS` and the iOS SDK. Or build inside Xcode.

## Build and run (outline)

1. Create an iOS app in Xcode (or use the scaffold in `platforms/ios/`).
2. Add the Vulkan (MoltenVK) framework and link your C++ Vulkan code (same `src/`, `include/`, `shaders/`).
3. In the app: create a `UIView`/`MTKView` or similar that gives you a drawable, create a `VkSurfaceKHR` from it via MoltenVK (e.g. `VK_MVK_ios_surface` or the appropriate iOS surface extension), then run your existing Vulkan init and render loop (e.g. on a CADisplayLink or Metal display link).

## Scaffold in this repo

- `platforms/ios/` — placeholder for an Xcode project or CMake-for-iOS setup that builds the Vulkan app for iOS and links it to a minimal single-view app.  
- You can copy the desktop `src/`, `include/`, `shaders/` and adapt: replace GLFW with iOS surface creation (MoltenVK iOS surface extension) and use the app’s main run loop.

## One window API for all (iOS included)

To use the same “window” API on desktop and iOS, use **SDL3** (or **SDL2**): both provide window + Vulkan surface on iOS (and Linux, Windows, macOS, Android). SDL3 is the modern option; SDL2 is the stable alternative. Your Vulkan code then only talks to SDL for the surface; no iOS-specific window code in the core.

## References

- [MoltenVK](https://github.com/KhronosGroup/MoltenVK)
- [Vulkan SDK macOS (includes MoltenVK)](https://vulkan.lunarg.com/sdk/home#mac)
- [VK_MVK_ios_surface](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_MVK_ios_surface.html)
