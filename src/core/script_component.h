/*
 * ScriptComponent — Behavior scripting component.
 * Supports both native C++ callbacks and Lua scripts.
 * 
 * Scripts provide custom game logic: AI, player control, gameplay mechanics.
 */
#pragma once

#include "component.h"
#include <cstdint>
#include <string>
#include <functional>
#include <unordered_map>
#include <any>

/**
 * Script type enumeration.
 */
enum class ScriptType : uint32_t {
    Native = 0,     /** C++ callback functions. */
    Lua,            /** Lua script file. */
    COUNT
};

/**
 * Script variable for runtime data exchange.
 */
struct ScriptVariable {
    enum class Type { Float, Int, Bool, String, Vec3, Ref };
    Type type = Type::Float;
    std::any value;
};

/**
 * Native script callbacks.
 * Use std::function for flexibility; can bind lambdas, member functions, etc.
 */
struct NativeScriptCallbacks {
    std::function<void()> onStart;
    std::function<void(float deltaTime)> onUpdate;
    std::function<void()> onDestroy;
    std::function<void(uint32_t otherObjectId)> onCollisionEnter;
    std::function<void(uint32_t otherObjectId)> onCollisionExit;
    std::function<void(uint32_t otherObjectId)> onTriggerEnter;
    std::function<void(uint32_t otherObjectId)> onTriggerExit;
};

/**
 * Lua script state (opaque handle).
 * Actual Lua state is managed by ScriptSystem.
 */
struct LuaScriptState {
    void* luaState = nullptr;           /** lua_State* */
    int tableRef = -1;                  /** Lua registry reference to script table. */
    bool bLoaded = false;
    std::string lastError;
};

/**
 * ScriptComponent — Attached to GameObjects for behavior.
 */
struct ScriptComponent {
    ScriptType type = ScriptType::Native;
    
    /** Path to script file (for Lua). */
    std::string scriptPath;
    
    /** Native callbacks (for Native type). */
    NativeScriptCallbacks nativeCallbacks;
    
    /** Lua state (for Lua type). */
    LuaScriptState luaState;
    
    /** Exposed variables for editor/serialization. */
    std::unordered_map<std::string, ScriptVariable> variables;
    
    /** Index of the owning GameObject. */
    uint32_t gameObjectIndex = 0;
    
    /** Script execution order (lower = earlier). */
    int32_t executionOrder = 0;
    
    /** Has OnStart been called? */
    bool bStarted = false;
};


