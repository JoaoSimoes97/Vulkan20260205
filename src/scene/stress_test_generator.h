/*
 * StressTestGenerator - Generate procedural scenes for instancing stress tests.
 * 
 * Creates thousands of objects with configurable parameters to benchmark
 * the multi-tier instancing system.
 */
#pragma once

#include "scene.h"
#include <cstdint>
#include <functional>

class MeshManager;
class MaterialManager;

/**
 * Parameters for stress test generation.
 */
struct StressTestParams {
    // Object counts per tier
    uint32_t staticCount = 10000;       // Terrain, props, flora
    uint32_t semiStaticCount = 500;     // Doors, switches, destructibles
    uint32_t dynamicCount = 200;        // NPCs, physics objects
    uint32_t proceduralCount = 1000;    // Particles (GPU-generated placeholder)
    
    // World bounds
    float worldSize = 200.0f;           // Half-size of world cube
    float heightVariation = 20.0f;      // Max height offset
    
    // Visual variety
    bool randomColors = true;           // Randomize object colors
    bool randomScales = true;           // Randomize object scales
    float minScale = 0.3f;
    float maxScale = 2.0f;
    
    // Seed for deterministic generation
    uint32_t seed = 12345;
    
    /** Quick presets */
    static StressTestParams Light()   { return { 1000, 100, 50, 200, 100.0f, 10.0f, true, true, 0.5f, 1.5f, 1 }; }
    static StressTestParams Medium()  { return { 10000, 500, 200, 1000, 200.0f, 20.0f, true, true, 0.3f, 2.0f, 42 }; }
    static StressTestParams Heavy()   { return { 50000, 2000, 1000, 5000, 500.0f, 50.0f, true, true, 0.2f, 3.0f, 999 }; }
    static StressTestParams Extreme() { return { 100000, 5000, 2000, 10000, 1000.0f, 100.0f, true, true, 0.1f, 4.0f, 7777 }; }
};

/**
 * Progress callback for long-running generation.
 * @param current Objects generated so far
 * @param total Total objects to generate
 * @return false to cancel generation
 */
using StressTestProgressCallback = std::function<bool(uint32_t current, uint32_t total)>;

/**
 * Generate a stress test scene with the given parameters.
 * 
 * @param scene Target scene (will be cleared first)
 * @param params Generation parameters
 * @param pMeshManager Mesh manager for procedural meshes
 * @param pMaterialManager Material manager for object materials
 * @param progressCallback Optional callback for progress updates
 * @return Total objects created
 */
uint32_t GenerateStressTestScene(
    Scene& scene,
    const StressTestParams& params,
    MeshManager* pMeshManager,
    MaterialManager* pMaterialManager,
    StressTestProgressCallback progressCallback = nullptr
);

/**
 * Get total object count for given params (without generating).
 */
inline uint32_t GetStressTestObjectCount(const StressTestParams& params) {
    return params.staticCount + params.semiStaticCount + 
           params.dynamicCount + params.proceduralCount;
}

/**
 * Get human-readable description of params.
 */
const char* GetStressTestPresetName(const StressTestParams& params);
