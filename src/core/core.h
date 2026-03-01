/*
 * Core Module — Entity-Component System
 * 
 * This is the heart of the engine's object model. Include this header
 * to access all core ECS types for creating and manipulating GameObjects.
 * 
 * Architecture:
 * - GameObject: Lightweight container with component indices
 * - Components: Data stored in Structure of Arrays (SoA) for cache efficiency
 * - Scene: Owns all component pools and provides query/update interfaces
 * 
 * Component Types:
 * - Transform: Position, rotation, scale (always present)
 * - Renderer: Mesh + Material for rendering
 * - Light: Light source (point, spot, directional)
 * - Physics: Rigid body + collider (future)
 * - Script: Behavior via Lua or C++ (future)
 * - Camera: Viewpoint for rendering (future)
 */
#pragma once

// Base component interface
#include "component.h"

// Core components
#include "transform.h"
#include "gameobject.h"
#include "renderer_component.h"
#include "light_component.h"
#include "camera_component.h"
#include "physics_component.h"
#include "script_component.h"

// Managers
#include "light_manager.h"

// Scene container (unified ECS + render list)
#include "scene/scene_unified.h"

