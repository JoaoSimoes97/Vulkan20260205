#include "vulkan_pipeline.h"
#include "vulkan_utils.h"
#include <stdexcept>

/*
 * Create: builds a graphics pipeline for the given device, extent, render pass, and shaders.
 * Fixed-function state is driven by pipelineParams (topology, rasterization, MSAA). Vertex input and
 * pipeline layout remain none; add when using vertex buffers or UBOs/textures.
 */
void VulkanPipeline::Create(VkDevice device, VkExtent2D extent, VkRenderPass renderPass,
                            VulkanShaderManager* pShaderManager,
                            const std::string& sVertPath, const std::string& sFragPath,
                            const GraphicsPipelineParams& pipelineParams) {
    VulkanUtils::LogTrace("VulkanPipeline::Create");
    if (device == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("VulkanPipeline::Create: invalid device");
        throw std::runtime_error("VulkanPipeline::Create: invalid device");
    }
    if ((pShaderManager == nullptr) || (pShaderManager->IsValid() == false)) {
        VulkanUtils::LogErr("VulkanPipeline::Create: invalid shader manager");
        throw std::runtime_error("VulkanPipeline::Create: invalid shader manager");
    }
    if (renderPass == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("VulkanPipeline::Create: invalid render pass");
        throw std::runtime_error("VulkanPipeline::Create: invalid render pass");
    }

    this->m_device = device;
    this->m_pShaderManager = pShaderManager;
    this->m_sVertPath = sVertPath;
    this->m_sFragPath = sFragPath;

    VkShaderModule modVert = this->m_pShaderManager->GetShader(device, sVertPath);
    VkShaderModule modFrag = this->m_pShaderManager->GetShader(device, sFragPath);
    if ((modVert == VK_NULL_HANDLE) || (modFrag == VK_NULL_HANDLE)) {
        if (modVert != VK_NULL_HANDLE)
            this->m_pShaderManager->Release(sVertPath);
        if (modFrag != VK_NULL_HANDLE)
            this->m_pShaderManager->Release(sFragPath);
        VulkanUtils::LogErr("VulkanPipeline::Create: failed to load shaders");
        throw std::runtime_error("VulkanPipeline::Create: failed to load shaders");
    }

    /* Vertex and fragment stages; entry point "main", no specialization. */
    VkPipelineShaderStageCreateInfo stVertStage = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext  = nullptr,
        .flags  = static_cast<VkPipelineShaderStageCreateFlags>(0),
        .stage  = VK_SHADER_STAGE_VERTEX_BIT,
        .module = modVert,
        .pName  = "main",
        .pSpecializationInfo = nullptr,
    };
    VkPipelineShaderStageCreateInfo stFragStage = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext  = nullptr,
        .flags  = static_cast<VkPipelineShaderStageCreateFlags>(0),
        .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = modFrag,
        .pName  = "main",
        .pSpecializationInfo = nullptr,
    };
    VkPipelineShaderStageCreateInfo vecStages[] = { stVertStage, stFragStage };

    /* No vertex bindings/attributes; shader uses gl_VertexIndex only (e.g. fullscreen tri). */
    VkPipelineVertexInputStateCreateInfo stVertexInput = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                           = nullptr,
        .flags                            = static_cast<VkPipelineVertexInputStateCreateFlags>(0),
        .vertexBindingDescriptionCount   = static_cast<uint32_t>(0),
        .pVertexBindingDescriptions      = nullptr,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(0),
        .pVertexAttributeDescriptions     = nullptr,
    };

    /* Input assembly: topology and primitive restart from pipelineParams. */
    VkPipelineInputAssemblyStateCreateInfo stInputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = static_cast<VkPipelineInputAssemblyStateCreateFlags>(0),
        .topology               = pipelineParams.topology,
        .primitiveRestartEnable = pipelineParams.primitiveRestartEnable,
    };

    /* Full extent viewport and scissor; depth [0, 1]. */
    VkViewport stViewport = {
        .x        = static_cast<float>(0.0),
        .y        = static_cast<float>(0.0),
        .width    = static_cast<float>(extent.width),
        .height   = static_cast<float>(extent.height),
        .minDepth = static_cast<float>(0.0),
        .maxDepth = static_cast<float>(1.0),
    };
    VkRect2D stScissor = {
        .offset = { .x = 0, .y = 0 },
        .extent = extent,
    };
    VkPipelineViewportStateCreateInfo stViewportState = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = static_cast<VkPipelineViewportStateCreateFlags>(0),
        .viewportCount = static_cast<uint32_t>(1),
        .pViewports    = &stViewport,
        .scissorCount  = static_cast<uint32_t>(1),
        .pScissors     = &stScissor,
    };

    /* Rasterization: polygon mode, cull, front face, line width from pipelineParams. */
    VkPipelineRasterizationStateCreateInfo stRaster = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = static_cast<VkPipelineRasterizationStateCreateFlags>(0),
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = pipelineParams.polygonMode,
        .cullMode                = pipelineParams.cullMode,
        .frontFace               = pipelineParams.frontFace,
        .depthBiasEnable         = VK_FALSE,
        .depthBiasConstantFactor = static_cast<float>(0.0),
        .depthBiasClamp          = static_cast<float>(0.0),
        .depthBiasSlopeFactor    = static_cast<float>(0.0),
        .lineWidth               = pipelineParams.lineWidth,
    };

    /* Multisample: sample count from pipelineParams (e.g. 1_BIT or 4_BIT for MSAA). */
    VkPipelineMultisampleStateCreateInfo stMultisample = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = static_cast<VkPipelineMultisampleStateCreateFlags>(0),
        .rasterizationSamples  = pipelineParams.rasterizationSamples,
        .sampleShadingEnable   = VK_FALSE,
        .minSampleShading      = static_cast<float>(1.0),
        .pSampleMask           = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable      = VK_FALSE,
    };

    /* Single color attachment: no blend, write RGBA. */
    VkPipelineColorBlendAttachmentState stBlendAttachment = {
        .blendEnable         = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
        .colorWriteMask      = static_cast<VkColorComponentFlags>(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT),
    };
    VkPipelineColorBlendStateCreateInfo stColorBlend = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = static_cast<VkPipelineColorBlendStateCreateFlags>(0),
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = static_cast<uint32_t>(1),
        .pAttachments    = &stBlendAttachment,
        .blendConstants  = { static_cast<float>(0.0), static_cast<float>(0.0), static_cast<float>(0.0), static_cast<float>(0.0) },
    };

    /* Layout: no descriptor sets, no push constants. Add when using UBOs/textures. */
    VkPipelineLayoutCreateInfo stLayoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = static_cast<VkPipelineLayoutCreateFlags>(0),
        .setLayoutCount         = static_cast<uint32_t>(0),
        .pSetLayouts            = nullptr,
        .pushConstantRangeCount = static_cast<uint32_t>(0),
        .pPushConstantRanges    = nullptr,
    };
    VkResult result = vkCreatePipelineLayout(device, &stLayoutInfo, nullptr, &this->m_pipelineLayout);
    if (result != VK_SUCCESS) {
        this->m_pShaderManager->Release(sVertPath);
        this->m_pShaderManager->Release(sFragPath);
        VulkanUtils::LogErr("vkCreatePipelineLayout failed: {}", static_cast<int>(result));
        throw std::runtime_error("VulkanPipeline::Create: pipeline layout failed");
    }

    /* Assemble graphics pipeline; no tessellation, no depth/stencil, subpass 0. */
    VkGraphicsPipelineCreateInfo stPipelineInfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = nullptr,
        .flags               = static_cast<VkPipelineCreateFlags>(0),
        .stageCount          = static_cast<uint32_t>(2),
        .pStages             = vecStages,
        .pVertexInputState   = &stVertexInput,
        .pInputAssemblyState = &stInputAssembly,
        .pTessellationState  = nullptr,
        .pViewportState      = &stViewportState,
        .pRasterizationState = &stRaster,
        .pMultisampleState   = &stMultisample,
        .pDepthStencilState  = nullptr,
        .pColorBlendState    = &stColorBlend,
        .pDynamicState       = nullptr,
        .layout              = this->m_pipelineLayout,
        .renderPass          = renderPass,
        .subpass             = static_cast<uint32_t>(0),
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = static_cast<int32_t>(-1),
    };
    result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, static_cast<uint32_t>(1), &stPipelineInfo, nullptr, &this->m_pipeline);
    if (result != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, this->m_pipelineLayout, nullptr);
        this->m_pipelineLayout = VK_NULL_HANDLE;
        this->m_pShaderManager->Release(sVertPath);
        this->m_pShaderManager->Release(sFragPath);
        VulkanUtils::LogErr("vkCreateGraphicsPipelines failed: {}", static_cast<int>(result));
        throw std::runtime_error("VulkanPipeline::Create: graphics pipeline failed");
    }
}

void VulkanPipeline::Destroy() {
    if (this->m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(this->m_device, this->m_pipeline, nullptr);
        this->m_pipeline = VK_NULL_HANDLE;
    }
    if (this->m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(this->m_device, this->m_pipelineLayout, nullptr);
        this->m_pipelineLayout = VK_NULL_HANDLE;
    }
    /* Release shader refs so VulkanShaderManager can unload if no other pipeline uses them. */
    if ((this->m_pShaderManager != nullptr) && (this->m_sVertPath.empty() == false))
        this->m_pShaderManager->Release(this->m_sVertPath);
    if ((this->m_pShaderManager != nullptr) && (this->m_sFragPath.empty() == false))
        this->m_pShaderManager->Release(this->m_sFragPath);
    this->m_pShaderManager = nullptr;
    this->m_sVertPath.clear();
    this->m_sFragPath.clear();
    this->m_device = VK_NULL_HANDLE;
}

VulkanPipeline::~VulkanPipeline() {
    this->Destroy();
}
