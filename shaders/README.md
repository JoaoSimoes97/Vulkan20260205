# Shaders

- **source/** — GLSL source (`.vert`, `.frag`). Edit these.
- **Compiled output** — CMake compiles to `build/shaders/*.spv` (and install copies to `install/shaders/`). The app loads `.spv` from the executable directory at runtime.

Build with glslc or glslangValidator; see CMakeLists.txt.
