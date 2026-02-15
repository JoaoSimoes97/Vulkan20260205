#include "vulkan_pipeline.h"
#include "vulkan_utils.h"
#include <stdexcept>

void VulkanPipeline::Create(VkDevice device, VkRenderPass renderPass,
                            VulkanShaderManager* pShaderManager,
                            const std::string& sVertPath, const std::string& sFragPath,
                            const GraphicsPipelineParams& pipelineParams,
                            const PipelineLayoutDescriptor& layoutDescriptor,
                            bool renderPassHasDepth) {
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
    this->m_pVertShader = pShaderManager->GetShader(device, sVertPath);
    this->m_pFragShader = pShaderManager->GetShader(device, sFragPath);
    if (!this->m_pVertShader || !this->m_pFragShader) {
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

    /* Single vertex binding: vec3 position per vertex (12 bytes). */
    const VkVertexInputBindingDescription vertexBinding = {
        .binding   = 0,
        .stride    = static_cast<uint32_t>(sizeof(float) * 3),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    const VkVertexInputAttributeDescription vertexAttribute = {
        .location = 0,
        .binding  = 0,
        .format   = VK_FORMAT_R32G32B32_SFLOAT,
        .offset   = 0,
    };
    VkPipelineVertexInputStateCreateInfo stVertexInput = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                           = nullptr,
        .flags                            = static_cast<VkPipelineVertexInputStateCreateFlags>(0),
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &vertexBinding,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions   = &vertexAttribute,
    };

    VkPipelineInputAssemblyStateCreateInfo stInputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = static_cast<VkPipelineInputAssemblyStateCreateFlags>(0),
        .topology               = pipelineParams.topology,
        .primitiveRestartEnable = pipelineParams.primitiveRestartEnable,
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
        .polygonMode             = pipelineParams.polygonMode,
        .cullMode                = pipelineParams.cullMode,
        .frontFace               = pipelineParams.frontFace,
        .depthBiasEnable         = VK_FALSE,
        .depthBiasConstantFactor = static_cast<float>(0.0),
        .depthBiasClamp          = static_cast<float>(0.0),
        .depthBiasSlopeFactor    = static_cast<float>(0.0),
        .lineWidth               = pipelineParams.lineWidth,
    };

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

    VkPipelineDepthStencilStateCreateInfo stDepthStencil = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .depthTestEnable       = pipelineParams.depthTestEnable,
        .depthWriteEnable      = pipelineParams.depthWriteEnable,
        .depthCompareOp        = pipelineParams.depthCompareOp,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_FALSE,
        .front                 = {},
        .back                  = {},
        .minDepthBounds        = 0.0f,
        .maxDepthBounds        = 1.0f,
    };
    VkPipelineDepthStencilStateCreateInfo* pDepthStencil = renderPassHasDepth ? &stDepthStencil : nullptr;

    VkPipelineLayoutCreateInfo stLayoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = static_cast<VkPipelineLayoutCreateFlags>(0),
        .setLayoutCount         = static_cast<uint32_t>(0),
        .pSetLayouts            = nullptr,
        .pushConstantRangeCount = static_cast<uint32_t>(layoutDescriptor.pushConstantRanges.size()),
        .pPushConstantRanges    = layoutDescriptor.pushConstantRanges.empty() ? nullptr : layoutDescriptor.pushConstantRanges.data(),
    };
    VkResult result = vkCreatePipelineLayout(device, &stLayoutInfo, nullptr, &this->m_pipelineLayout);
    if (result != VK_SUCCESS) {
        this->m_pVertShader.reset();
        this->m_pFragShader.reset();
        VulkanUtils::LogErr("vkCreatePipelineLayout failed: {}", static_cast<int>(result));
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
        .renderPass          = renderPass,
        .subpass             = static_cast<uint32_t>(0),
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = static_cast<int32_t>(-1),
    };
    result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, static_cast<uint32_t>(1), &stPipelineInfo, nullptr, &this->m_pipeline);
    if (result != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, this->m_pipelineLayout, nullptr);
        this->m_pipelineLayout = VK_NULL_HANDLE;
        this->m_pVertShader.reset();
        this->m_pFragShader.reset();
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
    this->m_pVertShader.reset();
    this->m_pFragShader.reset();
    this->m_device = VK_NULL_HANDLE;
}

VulkanPipeline::~VulkanPipeline() {
    this->Destroy();
}
