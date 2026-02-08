# Coding guidelines

## Comments

- **Single line only**, except for function/module descriptions.
- Use block comments: `/* This is a comment */`
- Function/module descriptions may span multiple lines and describe purpose, parameters, and return value.

## Naming

- **Functions**: Start with a **capital letter** (PascalCase). e.g. `InitWindow()`, `CreateInstance()`, `LogTrace()`.
- **Variables**: Start with a **lowercase letter or short type prefix** that indicates the type:
  - `l` — uint32_t (e.g. `lExtensionCount`, `lWidth`)
  - `n` — uint16_t (e.g. `nIndex`, `nPort`)
  - `c` — uint8_t, char (e.g. `cByte`, `cMask`)
  - `i` — int, int32_t (e.g. `iResult`, `iCount`)
  - `z` — size_t (e.g. `zSize`, `zLength`)
  - `s` — std::string or string-like (e.g. `sName`, `sPath`)
  - `b` — bool (e.g. `bQuit`, `bMinimized`)
  - `p` — pointer (e.g. `pWindow`, `pDevice`)
  - `vec` — std::vector (e.g. `vecExtensions`, `vecHandles`)
  - `f` — float (e.g. `fScale`, `fTime`)
  - `d` — double (e.g. `dValue`)
  - `e` — enum value (e.g. `eLevel`, `eSeverity`)
  - `st` — struct (e.g. `stApplicationInfo`, `stCreateInfo` for VkApplicationInfo, VkInstanceCreateInfo)
  - `evt` — event (e.g. `evt` for SDL_Event when a single event)
  - `u` — unsigned (generic, when not uint32/16/8) (e.g. `uFlags`)
- **Classes / types**: PascalCase (e.g. `VulkanApp`).
- **Constants / macros**: UPPER_SNAKE_CASE (e.g. `WINDOW_WIDTH`).

## Classes

- **Always use `this->`** when accessing member variables or member functions inside the class (e.g. `this->pWindow`, `this->InitVulkan()`).
- Order: public interface first, then private members, then private methods.

## Formatting

- **Indentation**: 4 spaces (no tabs).
- **Braces**: Opening brace on same line for functions and control flow (K&R style); closing brace on its own line.
- **Line length**: Prefer &lt; 100 characters; wrap with one indent for continuation.

## Files and includes

- **Headers**: Use `#pragma once`. Order: project headers first, then standard/library, alphabetical within each group.
- **Source**: Order includes: corresponding header first, then project headers, then standard/library.

## Other

- **Comparisons**: No use of logical NOT (`!`) in conditions; use explicit comparisons (e.g. `== false`, `== nullptr`, `== 0`, `== VK_FALSE`). Example: `if (ptr == nullptr)` instead of `if (!ptr)`; `if (SDL_Init(...) == false)` instead of `if (!SDL_Init(...))`.
- **Compound conditions**: Wrap every comparison in parentheses when using `&&` or `||`. Example: `(a != VK_NULL_HANDLE) || (b == 0)` instead of `a != VK_NULL_HANDLE || b == 0`; `(x == true) && (y == true)` instead of `x == true && y == true`.
- **Immediates / literals (all definitions)**: Every definition that uses a literal value must use an explicit cast. This includes all variable and constant initializers. Examples: `bool b = static_cast<bool>(true);`, `int i = static_cast<int>(800);`, `float f = static_cast<float>(1.0);`, `T* p = static_cast<T*>(nullptr);`. No bare literals like `= true`, `= 800`, `= 1.0f`, or `= nullptr`; always cast. Same for arguments passed to APIs.
- **Const correctness**: Use `const` for parameters and methods that do not modify state.
- **Magic numbers**: Prefer named constants (e.g. `constexpr int WINDOW_WIDTH = 800;`).
- **Error handling**: Prefer exceptions for fatal errors; log before throwing.
- **Scope**: Prefer the narrowest scope (e.g. declare loop variables in the loop).
