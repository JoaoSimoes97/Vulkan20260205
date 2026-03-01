/*
 * LightDebugRenderer — Wireframe visualization of lights in scene.
 * 
 * Renders:
 * - Point lights: 3 circles (wireframe sphere) + center cross
 * - Spot lights: Cone wireframe + direction arrow
 * - Directional lights: Arrow + sun symbol
 * 
 * Self-contained with own pipeline (LINE_LIST topology).
 */
#pragma once

#include "light_component.h"
#include "light_manager.h" // For EmissiveLightData
#include <vector>
#include <vulkan/vulkan.h>
#include <cstdint>

class Scene;  // unified scene

/**
 * Debug vertex: position (vec3) + color (vec3).
 */
struct DebugLineVertex {
    float position[3];
    float color[3];
};

/**
 * LightDebugRenderer — Self-contained debug line renderer for lights.
 */
class LightDebugRenderer {
public:
    LightDebugRenderer() = default;
    ~LightDebugRenderer() { Destroy(); }

    /**
     * Create Vulkan resources (pipeline, layout, shaders).
     * @param device Vulkan logical device.
     * @param renderPass Render pass compatible with debug drawing.
     * @param physicalDevice For memory allocation.
     * @return true on success.
     */
    bool Create(VkDevice device, VkRenderPass renderPass, VkPhysicalDevice physicalDevice);

    /**
     * Cleanup all Vulkan resources.
     */
    void Destroy();

    /**
     * Draw debug visualization for all lights in scene.
     * Call inside active render pass after main scene rendering.
     * @param cmd Active command buffer (inside render pass).
     * @param pScene Scene containing lights.
     * @param viewProjMatrix 4x4 view-projection matrix (column-major float[16]).
     */
    void Draw(VkCommandBuffer cmd, const Scene* pScene, const float* viewProjMatrix);

    /**
     * Set emissive lights for debug visualization.
     * Call before Draw() each frame with current emissive lights.
     */
    void SetEmissiveLights(const std::vector<EmissiveLightData>& emissiveLights) {
        m_emissiveLights = emissiveLights;
    }

    /**
     * Check if ready to render.
     */
    bool IsReady() const { return m_bReady; }

private:
    /* Generate geometry for each light type */
    void GeneratePointLightGeometry(std::vector<DebugLineVertex>& vertices,
                                     const float* position, float range, const float* color);
    void GenerateSpotLightGeometry(std::vector<DebugLineVertex>& vertices,
                                    const float* position, const float* direction,
                                    float range, float outerConeAngle, const float* color);
    void GenerateDirectionalLightGeometry(std::vector<DebugLineVertex>& vertices,
                                           const float* position, const float* direction,
                                           const float* color);

    /* Update vertex buffer with current frame's geometry */
    bool UpdateVertexBuffer(const std::vector<DebugLineVertex>& vertices);

    /* Vulkan handles */
    VkDevice            m_device            = VK_NULL_HANDLE;
    VkPhysicalDevice    m_physicalDevice    = VK_NULL_HANDLE;
    VkPipeline          m_pipeline          = VK_NULL_HANDLE;
    VkPipelineLayout    m_pipelineLayout    = VK_NULL_HANDLE;
    VkShaderModule      m_vertShader        = VK_NULL_HANDLE;
    VkShaderModule      m_fragShader        = VK_NULL_HANDLE;

    /* Dynamic vertex buffer */
    VkBuffer            m_vertexBuffer      = VK_NULL_HANDLE;
    VkDeviceMemory      m_vertexMemory      = VK_NULL_HANDLE;
    uint32_t            m_vertexCount       = 0;
    uint32_t            m_bufferCapacity    = 0;

    bool                m_bReady            = false;

    /* Emissive lights for debug rendering */
    std::vector<EmissiveLightData> m_emissiveLights;

    /* Constants */
    static constexpr uint32_t kCircleSegments = 24;
    static constexpr uint32_t kConeSegments = 12;
};
