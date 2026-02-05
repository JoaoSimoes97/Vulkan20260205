# iOS scaffold

Place here an Xcode project (or CMake-for-iOS build) that:

1. Builds the Vulkan app from the repo root (`../..`) for iOS (same `src/`, `include/`, `shaders/`), linking MoltenVK for iOS.
2. Provides a minimal app with a view that supplies a drawable for Vulkan/Metal.
3. Creates `VkSurfaceKHR` via the MoltenVK iOS surface extension and runs the same Vulkan init/render loop as desktop.

**What to install:** See [../../docs/platforms/ios.md](../../docs/platforms/ios.md).

**One API for window + Vulkan on all platforms:** Consider using SDL3 or SDL2 for iOS (and Android) so the same “window” code runs everywhere; see main README “Windowing: one API for all?”.
