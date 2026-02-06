# Vulkan validation layers

Project conventions for enabling and using validation layers (see also [Vulkan Tutorial: Validation layers](https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers)).

- **Debug only**: Enable validation layers when `NDEBUG` is not set (debug build); disable in release.
- **Layer**: Use `VK_LAYER_KHRONOS_validation` (LunarG SDK). Check support with `vkEnumerateInstanceLayerProperties` before enabling; throw if requested but unavailable.
- **Debug utils**: When validation is enabled, add `VK_EXT_debug_utils` and set up a debug messenger (callback for validation output). Pass `VkDebugUtilsMessengerCreateInfoEXT` in `pNext` of `VkInstanceCreateInfo` so `vkCreateInstance`/`vkDestroyInstance` are validated too.
- **Device**: Enable the same layer(s) at logical device creation for spec compatibility.
- **Cleanup**: Destroy the debug messenger before `vkDestroyInstance`.
