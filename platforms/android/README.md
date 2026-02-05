# Android scaffold

Place here an Android Studio project (or NDK CMake build) that:

1. Builds the shared Vulkan app from the repo root (`../..`) as a native library.
2. Provides one Activity that creates a `Surface` and passes it to native code.
3. In native code, creates `VkSurfaceKHR` via `VkAndroidSurfaceCreateInfoKHR` and runs the same Vulkan init/render loop as desktop.

**What to install:** See [../../docs/platforms/android.md](../../docs/platforms/android.md).

**One API for window + Vulkan on all platforms:** Consider using SDL3 or SDL2 for Android (and iOS) so the same “window” code runs everywhere; see README “Windowing: one API for all?”.
