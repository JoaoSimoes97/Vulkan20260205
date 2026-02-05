# Android: build and run

Same Vulkan code; build for Android with the NDK and a minimal Android app that creates a surface and runs the native Vulkan loop.

## What to install right now

1. **Android Studio** (or command-line NDK)
   - [Android Studio](https://developer.android.com/studio) (includes NDK)  
   - Or [NDK only](https://developer.android.com/ndk/downloads)

2. **CMake** (for Android)
   - Usually bundled with Android Studio / NDK

3. **Vulkan**
   - Android 7+ has Vulkan; no extra install. Headers/libs come from the NDK.

## Build and run (outline)

1. Create (or use) an Android project with **Native (C++)** support.
2. Point it at this repo’s C++ code and `CMakeLists.txt`, or add a CMake subproject that builds the Vulkan app as a shared library.
3. In the app: create a `Surface` (or use `SurfaceView`), get an `ANativeWindow` (or `AHardwareBuffer` / `VkAndroidSurfaceCreateInfoKHR`), create `VkSurfaceKHR`, then run your existing Vulkan init and render loop in a native thread.

## Scaffold in this repo

- `platforms/android/` — placeholder for an Android Studio project or NDK `CMakeLists.txt` that builds the Vulkan app for Android and links it to a minimal Activity.  
- You can copy the desktop `src/`, `include/`, `shaders/` and adapt: replace GLFW window creation with Android surface creation (`VkAndroidSurfaceCreateInfoKHR`), and use `android_main` instead of `main` if using `native_app_glue`.

## One window API for all (Android included)

To use the same “window” API on desktop and Android, use **SDL3** (or **SDL2**): both provide window + Vulkan surface on Android (and Linux, Windows, macOS, iOS). SDL3 is the modern option; SDL2 is the stable alternative. Your Vulkan code then only talks to SDL for the surface; no Android-specific window code in the core.

## References

- [Vulkan on Android](https://developer.android.com/ndk/guides/graphics/getting-started)
- [Android NDK CMake](https://developer.android.com/ndk/guides/cmake)
- [VkAndroidSurfaceCreateInfoKHR](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkAndroidSurfaceCreateInfoKHR.html)
