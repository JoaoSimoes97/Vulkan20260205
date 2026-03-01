/*
 * LightDebugRenderer — Full implementation.
 */
#include "light_debug_renderer.h"
#include "scene/scene_unified.h"
#include "light_component.h"
#include "transform.h"
#include "../vulkan/vulkan_utils.h"

#include <fstream>
#include <cmath>
#include <cstring>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- Load SPIR-V shader file ---- */
static std::vector<char> LoadShaderFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return {};
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
}

/* ---- Create shader module ---- */
static VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& code) {
    if (code.empty()) return VK_NULL_HANDLE;
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod = VK_NULL_HANDLE;
    vkCreateShaderModule(device, &ci, nullptr, &mod);
    return mod;
}

/* ---- Create ---- */
bool LightDebugRenderer::Create(VkDevice device, VkRenderPass renderPass, VkPhysicalDevice physicalDevice) {
    m_device = device;
    m_physicalDevice = physicalDevice;

    /* Load shaders */
    std::string vertPath = VulkanUtils::GetResourcePath("shaders/debug_line.vert.spv");
    std::string fragPath = VulkanUtils::GetResourcePath("shaders/debug_line.frag.spv");
    auto vertCode = LoadShaderFile(vertPath);
    auto fragCode = LoadShaderFile(fragPath);
    if (vertCode.empty() || fragCode.empty()) {
        VulkanUtils::LogWarn("LightDebugRenderer: Could not load debug_line shaders - debug rendering disabled");
        return false;
    }

    m_vertShader = CreateShaderModule(device, vertCode);
    m_fragShader = CreateShaderModule(device, fragCode);
    if (m_vertShader == VK_NULL_HANDLE || m_fragShader == VK_NULL_HANDLE) {
        VulkanUtils::LogWarn("LightDebugRenderer: Failed to create shader modules");
        Destroy();
        return false;
    }

    /* Pipeline layout: push constant = mat4 MVP (64 bytes) */
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = 64;

    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.pushConstantRangeCount = 1;
    layoutCI.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(device, &layoutCI, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        VulkanUtils::LogWarn("LightDebugRenderer: Failed to create pipeline layout");
        Destroy();
        return false;
    }

    /* Shader stages */
    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = m_vertShader;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = m_fragShader;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

    /* Vertex input: position (vec3) + color (vec3) */
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(DebugLineVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribs[2] = {};
    attribs[0].binding = 0;
    attribs[0].location = 0;
    attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribs[0].offset = offsetof(DebugLineVertex, position);
    attribs[1].binding = 0;
    attribs[1].location = 1;
    attribs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribs[1].offset = offsetof(DebugLineVertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions = attribs;

    /* Input assembly: LINE_LIST */
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    /* Viewport/scissor: dynamic */
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    /* Rasterizer: no culling */
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    /* Multisampling: none */
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    /* Depth: test but don't write */
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    /* Color blending: no blend */
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blendAttachment;

    /* Dynamic state: viewport, scissor */
    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynStates;

    /* Create pipeline */
    VkGraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = stages;
    pipelineCI.pVertexInputState = &vertexInput;
    pipelineCI.pInputAssemblyState = &inputAssembly;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pRasterizationState = &rasterizer;
    pipelineCI.pMultisampleState = &multisampling;
    pipelineCI.pDepthStencilState = &depthStencil;
    pipelineCI.pColorBlendState = &colorBlending;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.layout = m_pipelineLayout;
    pipelineCI.renderPass = renderPass;
    pipelineCI.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_pipeline) != VK_SUCCESS) {
        VulkanUtils::LogWarn("LightDebugRenderer: Failed to create pipeline");
        Destroy();
        return false;
    }

    m_bReady = true;
    VulkanUtils::LogInfo("LightDebugRenderer: Initialized successfully");
    return true;
}

/* ---- Destroy ---- */
void LightDebugRenderer::Destroy() {
    if (m_device == VK_NULL_HANDLE) return;

    if (m_vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
    }
    if (m_vertexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_vertexMemory, nullptr);
        m_vertexMemory = VK_NULL_HANDLE;
    }
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    if (m_vertShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device, m_vertShader, nullptr);
        m_vertShader = VK_NULL_HANDLE;
    }
    if (m_fragShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device, m_fragShader, nullptr);
        m_fragShader = VK_NULL_HANDLE;
    }

    m_bReady = false;
}

/* ---- Update vertex buffer ---- */
bool LightDebugRenderer::UpdateVertexBuffer(const std::vector<DebugLineVertex>& vertices) {
    if (vertices.empty()) { m_vertexCount = 0; return true; }

    VkDeviceSize bufferSize = vertices.size() * sizeof(DebugLineVertex);

    /* Reallocate if needed */
    if (m_bufferCapacity < vertices.size()) {
        if (m_vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
            vkFreeMemory(m_device, m_vertexMemory, nullptr);
        }

        uint32_t newCap = static_cast<uint32_t>(vertices.size() * 2);
        if (VulkanUtils::CreateBuffer(m_device, m_physicalDevice,
                newCap * sizeof(DebugLineVertex),
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &m_vertexBuffer, &m_vertexMemory) != VK_SUCCESS)
            return false;
        m_bufferCapacity = newCap;
    }

    /* Copy data */
    void* pData = nullptr;
    vkMapMemory(m_device, m_vertexMemory, 0, bufferSize, 0, &pData);
    std::memcpy(pData, vertices.data(), bufferSize);
    vkUnmapMemory(m_device, m_vertexMemory);

    m_vertexCount = static_cast<uint32_t>(vertices.size());
    return true;
}

/* ---- Generate point light geometry ---- */
void LightDebugRenderer::GeneratePointLightGeometry(std::vector<DebugLineVertex>& verts,
                                                     const float* pos, float range, const float* color) {
    const float PI = static_cast<float>(M_PI);

    /* 3 circles: XY, XZ, YZ planes */
    for (int plane = 0; plane < 3; ++plane) {
        for (uint32_t i = 0; i < kCircleSegments; ++i) {
            float a0 = (2.0f * PI * i) / kCircleSegments;
            float a1 = (2.0f * PI * (i + 1)) / kCircleSegments;

            DebugLineVertex v0{}, v1{};
            v0.color[0] = color[0]; v0.color[1] = color[1]; v0.color[2] = color[2];
            v1.color[0] = color[0]; v1.color[1] = color[1]; v1.color[2] = color[2];

            if (plane == 0) { /* XY */
                v0.position[0] = pos[0] + range * std::cos(a0);
                v0.position[1] = pos[1] + range * std::sin(a0);
                v0.position[2] = pos[2];
                v1.position[0] = pos[0] + range * std::cos(a1);
                v1.position[1] = pos[1] + range * std::sin(a1);
                v1.position[2] = pos[2];
            } else if (plane == 1) { /* XZ */
                v0.position[0] = pos[0] + range * std::cos(a0);
                v0.position[1] = pos[1];
                v0.position[2] = pos[2] + range * std::sin(a0);
                v1.position[0] = pos[0] + range * std::cos(a1);
                v1.position[1] = pos[1];
                v1.position[2] = pos[2] + range * std::sin(a1);
            } else { /* YZ */
                v0.position[0] = pos[0];
                v0.position[1] = pos[1] + range * std::cos(a0);
                v0.position[2] = pos[2] + range * std::sin(a0);
                v1.position[0] = pos[0];
                v1.position[1] = pos[1] + range * std::cos(a1);
                v1.position[2] = pos[2] + range * std::sin(a1);
            }
            verts.push_back(v0);
            verts.push_back(v1);
        }
    }

    /* Center cross (white) */
    float cs = range * 0.1f;
    DebugLineVertex cx0{}, cx1{}, cy0{}, cy1{}, cz0{}, cz1{};
    cx0.position[0] = pos[0] - cs; cx0.position[1] = pos[1]; cx0.position[2] = pos[2];
    cx1.position[0] = pos[0] + cs; cx1.position[1] = pos[1]; cx1.position[2] = pos[2];
    cy0.position[0] = pos[0]; cy0.position[1] = pos[1] - cs; cy0.position[2] = pos[2];
    cy1.position[0] = pos[0]; cy1.position[1] = pos[1] + cs; cy1.position[2] = pos[2];
    cz0.position[0] = pos[0]; cz0.position[1] = pos[1]; cz0.position[2] = pos[2] - cs;
    cz1.position[0] = pos[0]; cz1.position[1] = pos[1]; cz1.position[2] = pos[2] + cs;
    for (auto* v : {&cx0, &cx1, &cy0, &cy1, &cz0, &cz1}) { v->color[0] = 1.f; v->color[1] = 1.f; v->color[2] = 1.f; }
    verts.push_back(cx0); verts.push_back(cx1);
    verts.push_back(cy0); verts.push_back(cy1);
    verts.push_back(cz0); verts.push_back(cz1);
}

/* ---- Generate spot light geometry ---- */
void LightDebugRenderer::GenerateSpotLightGeometry(std::vector<DebugLineVertex>& verts,
                                                    const float* pos, const float* dir,
                                                    float range, float outerCone, const float* color) {
    const float PI = static_cast<float>(M_PI);
    float baseR = range * std::tan(outerCone);

    /* Normalize direction */
    float dLen = std::sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
    if (dLen < 0.001f) dLen = 1.f;
    float d[3] = { dir[0]/dLen, dir[1]/dLen, dir[2]/dLen };

    /* Find perpendicular axes */
    float up[3] = { 0.f, 1.f, 0.f };
    if (std::abs(d[1]) > 0.99f) { up[0] = 1.f; up[1] = 0.f; }

    float right[3] = {
        d[1]*up[2] - d[2]*up[1],
        d[2]*up[0] - d[0]*up[2],
        d[0]*up[1] - d[1]*up[0]
    };
    float rLen = std::sqrt(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
    if (rLen > 0.001f) { right[0] /= rLen; right[1] /= rLen; right[2] /= rLen; }

    float up2[3] = {
        right[1]*d[2] - right[2]*d[1],
        right[2]*d[0] - right[0]*d[2],
        right[0]*d[1] - right[1]*d[0]
    };

    float baseC[3] = { pos[0] + d[0]*range, pos[1] + d[1]*range, pos[2] + d[2]*range };

    /* Cone lines from apex to base */
    for (uint32_t i = 0; i < kConeSegments; ++i) {
        float a = (2.f * PI * i) / kConeSegments;
        float bp[3] = {
            baseC[0] + baseR * (right[0]*std::cos(a) + up2[0]*std::sin(a)),
            baseC[1] + baseR * (right[1]*std::cos(a) + up2[1]*std::sin(a)),
            baseC[2] + baseR * (right[2]*std::cos(a) + up2[2]*std::sin(a))
        };

        DebugLineVertex v0{}, v1{};
        v0.position[0] = pos[0]; v0.position[1] = pos[1]; v0.position[2] = pos[2];
        v1.position[0] = bp[0]; v1.position[1] = bp[1]; v1.position[2] = bp[2];
        v0.color[0] = color[0]; v0.color[1] = color[1]; v0.color[2] = color[2];
        v1.color[0] = color[0]*0.5f; v1.color[1] = color[1]*0.5f; v1.color[2] = color[2]*0.5f;
        verts.push_back(v0);
        verts.push_back(v1);
    }

    /* Base circle */
    for (uint32_t i = 0; i < kConeSegments; ++i) {
        float a0 = (2.f * PI * i) / kConeSegments;
        float a1 = (2.f * PI * (i+1)) / kConeSegments;
        float bp0[3] = {
            baseC[0] + baseR * (right[0]*std::cos(a0) + up2[0]*std::sin(a0)),
            baseC[1] + baseR * (right[1]*std::cos(a0) + up2[1]*std::sin(a0)),
            baseC[2] + baseR * (right[2]*std::cos(a0) + up2[2]*std::sin(a0))
        };
        float bp1[3] = {
            baseC[0] + baseR * (right[0]*std::cos(a1) + up2[0]*std::sin(a1)),
            baseC[1] + baseR * (right[1]*std::cos(a1) + up2[1]*std::sin(a1)),
            baseC[2] + baseR * (right[2]*std::cos(a1) + up2[2]*std::sin(a1))
        };
        DebugLineVertex v0{}, v1{};
        v0.position[0] = bp0[0]; v0.position[1] = bp0[1]; v0.position[2] = bp0[2];
        v1.position[0] = bp1[0]; v1.position[1] = bp1[1]; v1.position[2] = bp1[2];
        v0.color[0] = color[0]*0.5f; v0.color[1] = color[1]*0.5f; v0.color[2] = color[2]*0.5f;
        v1.color[0] = color[0]*0.5f; v1.color[1] = color[1]*0.5f; v1.color[2] = color[2]*0.5f;
        verts.push_back(v0);
        verts.push_back(v1);
    }
}

/* ---- Generate directional light geometry ---- */
void LightDebugRenderer::GenerateDirectionalLightGeometry(std::vector<DebugLineVertex>& verts,
                                                           const float* pos, const float* dir,
                                                           const float* color) {
    const float PI = static_cast<float>(M_PI);
    float dLen = std::sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
    if (dLen < 0.001f) dLen = 1.f;
    float d[3] = { dir[0]/dLen, dir[1]/dLen, dir[2]/dLen };

    float arrowLen = 3.0f;
    float end[3] = { pos[0] + d[0]*arrowLen, pos[1] + d[1]*arrowLen, pos[2] + d[2]*arrowLen };

    /* Main arrow shaft */
    DebugLineVertex v0{}, v1{};
    v0.position[0] = pos[0]; v0.position[1] = pos[1]; v0.position[2] = pos[2];
    v1.position[0] = end[0]; v1.position[1] = end[1]; v1.position[2] = end[2];
    v0.color[0] = color[0]; v0.color[1] = color[1]; v0.color[2] = color[2];
    v1.color[0] = color[0]; v1.color[1] = color[1]; v1.color[2] = color[2];
    verts.push_back(v0);
    verts.push_back(v1);

    /* Sun symbol at origin */
    float sunR = 0.5f;
    for (int i = 0; i < 8; ++i) {
        float a0 = (2.f * PI * i) / 8.f;
        float a1 = (2.f * PI * (i+1)) / 8.f;
        DebugLineVertex s0{}, s1{};
        s0.position[0] = pos[0] + sunR * std::cos(a0);
        s0.position[1] = pos[1] + sunR * std::sin(a0);
        s0.position[2] = pos[2];
        s1.position[0] = pos[0] + sunR * std::cos(a1);
        s1.position[1] = pos[1] + sunR * std::sin(a1);
        s1.position[2] = pos[2];
        s0.color[0] = 1.f; s0.color[1] = 1.f; s0.color[2] = 0.f;
        s1.color[0] = 1.f; s1.color[1] = 1.f; s1.color[2] = 0.f;
        verts.push_back(s0);
        verts.push_back(s1);
    }
    /* Sun rays */
    for (int i = 0; i < 8; ++i) {
        float a = (2.f * PI * i) / 8.f;
        DebugLineVertex r0{}, r1{};
        r0.position[0] = pos[0] + sunR * 1.1f * std::cos(a);
        r0.position[1] = pos[1] + sunR * 1.1f * std::sin(a);
        r0.position[2] = pos[2];
        r1.position[0] = pos[0] + sunR * 1.6f * std::cos(a);
        r1.position[1] = pos[1] + sunR * 1.6f * std::sin(a);
        r1.position[2] = pos[2];
        r0.color[0] = 1.f; r0.color[1] = 1.f; r0.color[2] = 0.f;
        r1.color[0] = 1.f; r1.color[1] = 0.8f; r1.color[2] = 0.f;
        verts.push_back(r0);
        verts.push_back(r1);
    }
}

/* ---- Draw ---- */
void LightDebugRenderer::Draw(VkCommandBuffer cmd, const Scene* pScene, const float* viewProjMatrix) {
    if (!m_bReady || pScene == nullptr) return;

    std::vector<DebugLineVertex> vertices;
    vertices.reserve(2048);

    const auto& lights = pScene->GetLights();
    const auto& transforms = pScene->GetTransforms();
    const auto& gameObjects = pScene->GetGameObjects();

    for (size_t li = 0; li < lights.size(); ++li) {
        const LightComponent& light = lights[li];

        /* Find transform for this light */
        float pos[3] = { 0.f, 0.f, 0.f };
        float dir[3] = { 0.f, -1.f, 0.f };

        for (const auto& go : gameObjects) {
            if ((go.lightIndex == li) && (go.transformIndex != INVALID_COMPONENT_INDEX)) {
                const Transform& xf = transforms[static_cast<size_t>(go.transformIndex)];
                pos[0] = xf.position[0]; pos[1] = xf.position[1]; pos[2] = xf.position[2];
                /* Derive direction from transform rotation (forward = -Z local axis). */
                TransformGetForward(xf, dir[0], dir[1], dir[2]);
                break;
            }
        }

        switch (light.type) {
            case LightType::Point:
                GeneratePointLightGeometry(vertices, pos, light.range, light.color);
                break;
            case LightType::Spot:
                GenerateSpotLightGeometry(vertices, pos, dir, light.range, light.outerConeAngle, light.color);
                break;
            case LightType::Directional:
                GenerateDirectionalLightGeometry(vertices, pos, dir, light.color);
                break;
            case LightType::Area:
                /* Area lights: draw as point for now (no geometry yet). */
                GeneratePointLightGeometry(vertices, pos, light.range, light.color);
                break;
            case LightType::COUNT:
                break;
        }
    }
    
    // Draw emissive lights as point lights with orange tint to distinguish
    for (const EmissiveLightData& emissive : m_emissiveLights) {
        // Use emissive color with slight orange tint for distinction
        float emissiveDebugColor[3] = {
            emissive.color[0] * 0.8f + 0.2f, // Add some yellow/orange
            emissive.color[1] * 0.8f + 0.1f,
            emissive.color[2] * 0.5f         // Reduce blue for warmth
        };
        GeneratePointLightGeometry(vertices, emissive.position, emissive.radius, emissiveDebugColor);
    }

    if (vertices.empty()) return;
    if (!UpdateVertexBuffer(vertices)) return;

    /* Bind pipeline */
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    /* Push constants: MVP */
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, viewProjMatrix);

    /* Bind vertex buffer and draw */
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertexBuffer, &offset);
    vkCmdDraw(cmd, m_vertexCount, 1, 0, 0);
}
