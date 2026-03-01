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

## Parameters and arguments (input/output suffixes)

All function parameters and call-site arguments must be clearly identifiable as input, output, or in-out. Use the following suffixes in addition to the type prefix (e.g. `p`, `vec`, `s`):

- **`_ic`** — **input const**: read-only input. Use for `const T&`, `const T*`, and value parameters that are not modified (e.g. `sPath_ic`, `pScene_ic`, `vecData_ic`).
- **`_in`** — **input**: non-const input that the function may read but does not take ownership of or use as primary output (e.g. when a non-const reference is required by an API but the logical role is input).
- **`_out`** — **output**: parameter that the function writes into; caller provides storage (e.g. `vecOutDrawCalls_out`, `pResult_out`). The type prefix still applies (e.g. `vec` for vector).
- **`_io`** — **in-out**: parameter that is both read and written by the function (e.g. `stState_io`).

Examples:

- `void Build(std::vector<DrawCall>& vecOutDrawCalls_out, const Scene* pScene_ic, const float* pViewProj_ic);`
- `void OnCompletedLoad(LoadJobType eType_ic, const std::string& sPath_ic, const std::vector<uint8_t>& vecData_ic);`
- `bool Parse(const uint8_t* pData_ic, size_t zSize_ic, std::vector<float>& vecPositions_out, uint32_t& lVertexCount_out);`

When the name already implies direction (e.g. `outDrawCalls`), the suffix still applies for consistency: `vecOutDrawCalls_out`.

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
- **Forward declarations**: Prefer forward declarations in headers when a type is only used as pointer/reference; include the full header in the `.cpp` that needs the definition. Reduces compile time and coupling.

## Other

- **Comparisons**: No use of logical NOT (`!`) in conditions; use explicit comparisons (e.g. `== false`, `== nullptr`, `== 0`, `== VK_FALSE`). Example: `if (ptr == nullptr)` instead of `if (!ptr)`; `if (SDL_Init(...) == false)` instead of `if (!SDL_Init(...))`.
- **Compound conditions**: Wrap every comparison in parentheses when using `&&` or `||`. Example: `(a != VK_NULL_HANDLE) || (b == 0)` instead of `a != VK_NULL_HANDLE || b == 0`; `(x == true) && (y == true)` instead of `x == true && y == true`.
- **Immediates / literals (all definitions)**: Every definition that uses a literal value must use an explicit cast. This includes all variable and constant initializers. Examples: `bool b = static_cast<bool>(true);`, `int i = static_cast<int>(800);`, `float f = static_cast<float>(1.0);`, `T* p = static_cast<T*>(nullptr);`. No bare literals like `= true`, `= 800`, `= 1.0f`, or `= nullptr`; always cast. Same for arguments passed to APIs.
- **Const correctness**: Use `const` for parameters and methods that do not modify state.
- **Magic numbers**: Prefer named constants (e.g. `constexpr int WINDOW_WIDTH = 800;`).
- **Error handling**: Fatal: log (e.g. VulkanUtils::LogErr) then throw. Recoverable: return false and log; no silent ignores.
- **Scope**: Prefer the narrowest scope (e.g. declare loop variables in the loop).

## Vulkan

Validation layer setup: [docs/vulkan/validation-layers.md](../vulkan/validation-layers.md).

## Commit messages

- **Short**: One concise line (about 50–72 chars); optional second line for detail.
- **Imperative**: Start with a verb, e.g. "Add …", "Fix …", "Refactor …" (not "Added" or "Fixes").
- **Optional prefix**: `feat:`, `fix:`, `refactor:`, `docs:` etc. for scope (e.g. `feat(vulkan): add validation layers`).

## Git

- **One logical change per commit**: Easier to review and revert.
- **Docs**: Update README or relevant docs when adding features, changing setup, or changing behaviour users rely on.

## C++ style (additional)

- **No lambda functions**: Do not use lambda expressions (e.g. `[this](...) { ... }`). Use named functions or static member functions instead. If a callback or one-off logic is needed, define a private member function or a free function in an anonymous namespace and pass it by address (e.g. `std::function` or function pointer). This improves readability, testability, and stack traces, and keeps the guidelines for naming and parameter suffixes applicable.
- **auto**: Prefer `auto` for range-for and when the type is obvious from context; keeps names consistent with type prefixes.
- **Avoid redundant API calls**: Cache results when you would otherwise call the same Vulkan (or other) API again with the same inputs in the same scope.
