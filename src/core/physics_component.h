/*
 * PhysicsComponent — Physics simulation component.
 * Stores rigid body state, collider, and physics material properties.
 * 
 * FUTURE: Will integrate with physics engine (Jolt, Bullet, custom).
 * Not in alpha: stub only; no creation or simulation path.
 */
#pragma once

#include "component.h"
#include <cstdint>

/**
 * Rigid body type enumeration.
 */
enum class RigidBodyType : uint32_t {
    Static = 0,     /** Doesn't move, infinite mass. */
    Dynamic,        /** Fully simulated with forces and collisions. */
    Kinematic,      /** Moved by code, affects dynamic bodies. */
    COUNT
};

/**
 * Collider shape enumeration.
 */
enum class ColliderShape : uint32_t {
    Sphere = 0,
    Box,
    Capsule,
    Mesh,           /** Convex hull or triangle mesh. */
    COUNT
};

/**
 * Physics material — surface properties.
 */
struct PhysicsMaterial {
    float friction = 0.5f;          /** Friction coefficient (0 = ice, 1 = rubber). */
    float restitution = 0.3f;       /** Bounciness (0 = no bounce, 1 = perfect bounce). */
    float density = 1.0f;           /** Density for mass calculation. */
};

/**
 * Rigid body state — position, velocity, forces.
 */
struct RigidBodyState {
    RigidBodyType type = RigidBodyType::Dynamic;
    
    float mass = 1.0f;
    float invMass = 1.0f;           /** Cached inverse mass (0 for static). */
    
    float linearDamping = 0.05f;    /** Linear velocity decay. */
    float angularDamping = 0.05f;   /** Angular velocity decay. */
    
    float linearVelocity[3] = {0, 0, 0};
    float angularVelocity[3] = {0, 0, 0};
    
    float accumForce[3] = {0, 0, 0};    /** Accumulated force this frame. */
    float accumTorque[3] = {0, 0, 0};   /** Accumulated torque this frame. */
    
    bool bGravityEnabled = true;
    bool bSimulationEnabled = true;
};

/**
 * Collider data — shape and dimensions.
 */
struct ColliderData {
    ColliderShape shape = ColliderShape::Sphere;
    
    float radius = 0.5f;            /** Sphere/capsule radius. */
    float halfExtents[3] = {0.5f, 0.5f, 0.5f};  /** Box half-extents. */
    float height = 1.0f;            /** Capsule height. */
    
    float offset[3] = {0, 0, 0};    /** Local offset from transform center. */
    
    bool bIsTrigger = false;        /** Trigger = no physics response, only events. */
    uint32_t collisionMask = 0xFFFFFFFF;    /** Collision layer mask. */
};

/**
 * PhysicsComponent — Attached to GameObjects for physics simulation.
 */
struct PhysicsComponent {
    RigidBodyState rigidBody;
    ColliderData collider;
    PhysicsMaterial material;
    
    /** Index of the owning GameObject. */
    uint32_t gameObjectIndex = 0;
    
    /** Dirty flag for transform sync. */
    bool bTransformDirty = false;
};


