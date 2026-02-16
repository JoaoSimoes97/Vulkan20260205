#include "vulkan_pipeline.h"
#include "vulkan_utils.h"
#include <stdexcept>

void VulkanPipeline::Create(VkDevice pDevice_ic, VkRenderPass renderPass_ic,
                            VulkanShaderManager* pShaderManager_ic,
                            const std::string& sVertPath_ic, const std::string& sFragPath_ic,
                            const GraphicsPipelineParams& stPipelineParams_ic,
                            const PipelineLayoutDescriptor& stLayoutDescriptor_ic,
                            bool bRenderPassHasDepth_ic) {
    VulkanUtils::LogTrace("VulkanPipeline::Create");
    if (pDevice_ic == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("VulkanPipeline::Create: invalid device");
        throw std::runtime_error("VulkanPipeline::Create: invalid device");
    }
    if ((pShaderManager_ic == nullptr) || (pShaderManager_ic->IsValid() == false)) {
        VulkanUtils::LogErr("VulkanPipeline::Create: invalid shader manager");
        throw std::runtime_error("VulkanPipeline::Create: invalid shader manager");
    }
    if (renderPass_ic == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("VulkanPipeline::Create: invalid render pass");
        throw std::runtime_error("VulkanPipeline::Create: invalid render pass");
    }

    this->m_device = pDevice_ic;
    this->m_pVertShader = pShaderManager_ic->GetShader(pDevice_ic, sVertPath_ic);
    this->m_pFragShader = pShaderManager_ic->GetShader(pDevice_ic, sFragPath_ic);
    if ((this->m_pVertShader == nullptr) || (this->m_pFragShader == nullptr)) {
        this->m_pVertShader.reset();
        this->m_pFragShader.reset();
        VulkanUtils::LogErr("VulkanPipeline::Create: failed to load shaders");
        throw std::runtime_error("VulkanPipeline::Create: failed to load shaders");
    }

    VkShaderModule modVert = *this->m_pVertShader.get();
    VkShaderModule modFrag = *this->m_pFragShader.get();

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

    /* Single vertex binding: interleaved position + UV + normal (32 bytes per vertex). */
    const VkVertexInputBindingDescription vertexBinding = {
        .binding   = 0,
        .stride    = 32u, // sizeof(VertexData) = 3*float (pos) + 2*float (UV) + 3*float (normal)
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    const VkVertexInputAttributeDescription vertexAttributes[] = {
        {
            .location = 0,
            .binding  = 0,
            .format   = VK_FORMAT_R32G32B32_SFLOAT, // position
            .offset   = 0,
        },
        {
            .location = 1,
            .binding  = 0,
            .format   = VK_FORMAT_R32G32_SFLOAT,    // UV
            .offset   = 12,
        },
        {
            .location = 2,
            .binding  = 0,
            .format   = VK_FORMAT_R32G32B32_SFLOAT, // normal
            .offset   = 20,
        },
    };
    VkPipelineVertexInputStateCreateInfo stVertexInput = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                           = nullptr,
        .flags                            = static_cast<VkPipelineVertexInputStateCreateFlags>(0),
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &vertexBinding,
        .vertexAttributeDescriptionCount = 3,
        .pVertexAttributeDescriptions   = vertexAttributes,
    };

    VkPipelineInputAssemblyStateCreateInfo stInputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = static_cast<VkPipelineInputAssemblyStateCreateFlags>(0),
        .topology               = stPipelineParams_ic.topology,
        .primitiveRestartEnable = stPipelineParams_ic.primitiveRestartEnable,
    };

    VkPipelineViewportStateCreateInfo stViewportState = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = static_cast<VkPipelineViewportStateCreateFlags>(0),
        .viewportCount = static_cast<uint32_t>(1),
        .pViewports    = nullptr,
        .scissorCount  = static_cast<uint32_t>(1),
        .pScissors     = nullptr,
    };

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo stDynamicState = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext             = nullptr,
        .flags             = static_cast<VkPipelineDynamicStateCreateFlags>(0),
        .dynamicStateCount = static_cast<uint32_t>(2),
        .pDynamicStates    = dynamicStates,
    };

    VkPipelineRasterizationStateCreateInfo stRaster = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = static_cast<VkPipelineRasterizationStateCreateFlags>(0),
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = stPipelineParams_ic.polygonMode,
        .cullMode                = stPipelineParams_ic.cullMode,
        .frontFace               = stPipelineParams_ic.frontFace,
        .depthBiasEnable         = VK_FALSE,
        .depthBiasConstantFactor = static_cast<float>(0.0),
        .depthBiasClamp          = static_cast<float>(0.0),
        .depthBiasSlopeFactor    = static_cast<float>(0.0),
        .lineWidth               = stPipelineParams_ic.lineWidth,
    };

    VkPipelineMultisampleStateCreateInfo stMultisample = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = static_cast<VkPipelineMultisampleStateCreateFlags>(0),
        .rasterizationSamples  = stPipelineParams_ic.rasterizationSamples,
        .sampleShadingEnable   = VK_FALSE,
        .minSampleShading      = static_cast<float>(1.0),
        .pSampleMask           = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable      = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState stBlendAttachment = {
        .blendEnable         = stPipelineParams_ic.blendEnable,
        .srcColorBlendFactor = stPipelineParams_ic.srcColorBlendFactor,
        .dstColorBlendFactor = stPipelineParams_ic.dstColorBlendFactor,
        .colorBlendOp        = stPipelineParams_ic.colorBlendOp,
        .srcAlphaBlendFactor = stPipelineParams_ic.srcAlphaBlendFactor,
        .dstAlphaBlendFactor = stPipelineParams_ic.dstAlphaBlendFactor,
        .alphaBlendOp        = stPipelineParams_ic.alphaBlendOp,
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

    VkPipelineDepthStencilStateCreateInfo stDepthStencil = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .depthTestEnable       = stPipelineParams_ic.depthTestEnable,
        .depthWriteEnable      = stPipelineParams_ic.depthWriteEnable,
        .depthCompareOp        = stPipelineParams_ic.depthCompareOp,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_FALSE,
        .front                 = {},
        .back                  = {},
        .minDepthBounds        = 0.0f,
        .maxDepthBounds        = 1.0f,
    };
    VkPipelineDepthStencilStateCreateInfo* pDepthStencil = (bRenderPassHasDepth_ic == true) ? &stDepthStencil : nullptr;

    VkPipelineLayoutCreateInfo stLayoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = static_cast<VkPipelineLayoutCreateFlags>(0),
        .setLayoutCount         = static_cast<uint32_t>(stLayoutDescriptor_ic.descriptorSetLayouts.size()),
        .pSetLayouts            = (stLayoutDescriptor_ic.descriptorSetLayouts.empty() == true) ? nullptr : stLayoutDescriptor_ic.descriptorSetLayouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(stLayoutDescriptor_ic.pushConstantRanges.size()),
        .pPushConstantRanges    = (stLayoutDescriptor_ic.pushConstantRanges.empty() == true) ? nullptr : stLayoutDescriptor_ic.pushConstantRanges.data(),
    };
    VkResult r = vkCreatePipelineLayout(pDevice_ic, &stLayoutInfo, nullptr, &this->m_pipelineLayout);
    if (r != VK_SUCCESS) {
        this->m_pVertShader.reset();
        this->m_pFragShader.reset();
        VulkanUtils::LogErr("vkCreatePipelineLayout failed: {}", static_cast<int>(r));
        throw std::runtime_error("VulkanPipeline::Create: pipeline layout failed");
    }

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
        .pDepthStencilState  = pDepthStencil,
        .pColorBlendState    = &stColorBlend,
        .pDynamicState       = &stDynamicState,
        .layout              = this->m_pipelineLayout,
        .renderPass          = renderPass_ic,
        .subpass             = static_cast<uint32_t>(0),
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = static_cast<int32_t>(-1),
    };
    r = vkCreateGraphicsPipelines(pDevice_ic, VK_NULL_HANDLE, static_cast<uint32_t>(1), &stPipelineInfo, nullptr, &this->m_pipeline);
    if (r != VK_SUCCESS) {
        vkDestroyPipelineLayout(pDevice_ic, this->m_pipelineLayout, nullptr);
        this->m_pipelineLayout = VK_NULL_HANDLE;
        this->m_pVertShader.reset();
        this->m_pFragShader.reset();
        VulkanUtils::LogErr("vkCreateGraphicsPipelines failed: {}", static_cast<int>(r));
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
    this->m_pVertShader.reset();
    this->m_pFragShader.reset();
    this->m_device = VK_NULL_HANDLE;
}

VulkanPipeline::~VulkanPipeline() {
    this->Destroy();
}
