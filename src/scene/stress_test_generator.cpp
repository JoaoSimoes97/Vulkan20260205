/*
 * StressTestGenerator - Implementation
 */
#include "stress_test_generator.h"
#include "scene_unified.h"
#include "object.h"
#include "managers/mesh_manager.h"
#include "managers/material_manager.h"
#include <cmath>
#include <random>

namespace {
    // Simple fast xorshift random
    class FastRandom {
    public:
        explicit FastRandom(uint32_t seed) : state(seed ? seed : 1) {}
        
        uint32_t Next() {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }
        
        float NextFloat() { return static_cast<float>(Next()) / static_cast<float>(0xFFFFFFFFu); }
        float NextFloat(float min, float max) { return min + NextFloat() * (max - min); }
        
    private:
        uint32_t state;
    };
    
    // Generate a random color with good saturation
    void RandomColor(FastRandom& rng, float* rgba) {
        // Use HSV with high saturation and convert to RGB
        float h = rng.NextFloat() * 6.0f;
        float s = rng.NextFloat(0.6f, 1.0f);
        float v = rng.NextFloat(0.5f, 1.0f);
        
        int i = static_cast<int>(h);
        float f = h - static_cast<float>(i);
        float p = v * (1.0f - s);
        float q = v * (1.0f - s * f);
        float t = v * (1.0f - s * (1.0f - f));
        
        switch (i % 6) {
            case 0: rgba[0] = v; rgba[1] = t; rgba[2] = p; break;
            case 1: rgba[0] = q; rgba[1] = v; rgba[2] = p; break;
            case 2: rgba[0] = p; rgba[1] = v; rgba[2] = t; break;
            case 3: rgba[0] = p; rgba[1] = q; rgba[2] = v; break;
            case 4: rgba[0] = t; rgba[1] = p; rgba[2] = v; break;
            default: rgba[0] = v; rgba[1] = p; rgba[2] = q; break;
        }
        rgba[3] = 1.0f;
    }
    
    // Generate random position within world bounds
    void GeneratePosition(FastRandom& rng, float worldSize, float heightVar, float& px, float& py, float& pz) {
        px = rng.NextFloat(-worldSize, worldSize);
        py = rng.NextFloat(0.0f, heightVar);
        pz = rng.NextFloat(-worldSize, worldSize);
    }
    
    // Generate random rotation quaternion (Y-axis rotation for props)
    void RandomRotation(FastRandom& rng, float& qx, float& qy, float& qz, float& qw) {
        float angle = rng.NextFloat() * 6.28318f;
        qx = 0.0f;
        qy = std::sin(angle * 0.5f);
        qz = 0.0f;
        qw = std::cos(angle * 0.5f);
    }
}

uint32_t GenerateStressTestScene(
    Scene& scene,
    const StressTestParams& params,
    MeshManager* pMeshManager,
    MaterialManager* pMaterialManager,
    StressTestProgressCallback progressCallback
) {
    if (!pMeshManager || !pMaterialManager) return 0;
    
    scene.Clear();
    scene.SetName("Stress Test");
    
    uint32_t totalCount = GetStressTestObjectCount(params);
    uint32_t created = 0;
    
    // Use cube mesh for all stress test objects
    auto cubeMesh = pMeshManager->GetOrCreateProcedural("cube");
    
    // Get untextured material for procedural objects
    auto defaultMaterial = pMaterialManager->GetMaterial("main_untex");
    
    // Lambda to create objects of a specific tier
    // Each tier gets its own RNG offset so different tiers don't overlap spatially
    auto createObjects = [&](uint32_t count, InstanceTier tier, const char* namePrefix, uint32_t tierSeedOffset) {
        FastRandom tierRng(params.seed + tierSeedOffset);  // Unique RNG per tier
        
        for (uint32_t i = 0; i < count && created < totalCount; ++i) {
            Object obj;
            obj.name = std::string(namePrefix) + "_" + std::to_string(i);
            obj.instanceTier = tier;
            
            obj.pMesh = cubeMesh;
            obj.pMaterial = defaultMaterial;
            
            // Random position (using tier-specific RNG)
            float px, py, pz;
            GeneratePosition(tierRng, params.worldSize, params.heightVariation, px, py, pz);
            
            // Random rotation
            float qx, qy, qz, qw;
            RandomRotation(tierRng, qx, qy, qz, qw);
            
            // Random scale
            float scale = 1.0f;
            if (params.randomScales) {
                scale = tierRng.NextFloat(params.minScale, params.maxScale);
            }
            
            // Random color
            if (params.randomColors) {
                RandomColor(tierRng, obj.color);
            } else {
                obj.color[0] = obj.color[1] = obj.color[2] = obj.color[3] = 1.0f;
            }
            
            // Build local transform matrix using helper
            ObjectSetFromPositionRotationScale(obj.localTransform,
                px, py, pz,
                qx, qy, qz, qw,
                scale, scale, scale);
            
            scene.AddObject(std::move(obj));
            ++created;
            
            // Progress callback every 1000 objects
            if (progressCallback && (created % 1000 == 0)) {
                if (!progressCallback(created, totalCount)) {
                    return; // Cancelled
                }
            }
        }
    };
    
    // Generate objects for each tier with unique seed offsets
    // Offsets are prime numbers * 1000000 to ensure non-overlapping RNG sequences
    createObjects(params.staticCount, InstanceTier::Static, "Static", 0);
    createObjects(params.semiStaticCount, InstanceTier::SemiStatic, "SemiStatic", 1000003);
    createObjects(params.dynamicCount, InstanceTier::Dynamic, "Dynamic", 2000011);
    createObjects(params.proceduralCount, InstanceTier::Procedural, "Procedural", 3000017);
    
    // Add floor
    {
        Object floor;
        floor.name = "StressTest_Floor";
        floor.instanceTier = InstanceTier::Static;
        floor.pMesh = pMeshManager->GetOrCreateProcedural("rectangle");
        floor.pMaterial = defaultMaterial;
        floor.color[0] = 0.3f; floor.color[1] = 0.35f; floor.color[2] = 0.3f; floor.color[3] = 1.0f;
        
        // Floor: rotated 90 degrees, scaled to world size
        ObjectSetFromPositionRotationScale(floor.localTransform,
            0.0f, -0.5f, 0.0f,
            -0.707f, 0.0f, 0.0f, 0.707f,  // 90 degree rotation around X
            params.worldSize * 2.0f, params.worldSize * 2.0f, 1.0f);
        
        scene.AddObject(std::move(floor));
        ++created;
    }
    
    // Final progress callback
    if (progressCallback) {
        progressCallback(created, totalCount);
    }
    
    scene.MarkDirty();
    return created;
}

const char* GetStressTestPresetName(const StressTestParams& params) {
    uint32_t total = GetStressTestObjectCount(params);
    if (total <= 1500) return "Light (~1.3K)";
    if (total <= 12000) return "Medium (~12K)";
    if (total <= 60000) return "Heavy (~58K)";
    return "Extreme (~117K)";
}
