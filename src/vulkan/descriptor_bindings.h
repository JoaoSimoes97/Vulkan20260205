#pragma once

#include <cstdint>

/**
 * Central descriptor set 0 binding numbers for main PBR pipeline.
 * Must match shaders (vert.vert, frag.frag) and VulkanApp descriptor set layout.
 * Use these constants when adding new pipelines or bindings to avoid mismatches.
 */
namespace DescriptorBindings {
namespace Set0 {
    constexpr uint32_t kBaseColorTexture     = 0u;
    constexpr uint32_t kGlobalUBO            = 1u;
    constexpr uint32_t kObjectDataSSBO       = 2u;
    constexpr uint32_t kLightDataSSBO        = 3u;
    constexpr uint32_t kMetallicRoughnessTex = 4u;
    constexpr uint32_t kEmissiveTex          = 5u;
    constexpr uint32_t kNormalTex            = 6u;
    constexpr uint32_t kOcclusionTex         = 7u;
    constexpr uint32_t kVisibleIndicesSSBO   = 8u;
}
}
