#include "vulkan_pipeline.h"
#include "vulkan_utils.h"
#include <stdexcept>

void VulkanPipeline::Create(VkDevice device, VkExtent2D extent, VkRenderPass renderPass) {
    VulkanUtils::LogTrace("VulkanPipeline::Create");
    (void)extent;
    (void)renderPass;
    if (device == VK_NULL_HANDLE) {
        VulkanUtils::LogErr("VulkanPipeline::Create: invalid device");
        throw std::runtime_error("VulkanPipeline::Create: invalid device");
    }
    m_device = device;

    /* TODO: shader modules, pipeline layout, graphics pipeline (vert/frag), vkCreateGraphicsPipeline. */
    m_pipeline = VK_NULL_HANDLE;
    m_pipelineLayout = VK_NULL_HANDLE;
}

void VulkanPipeline::Destroy() {
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    m_device = VK_NULL_HANDLE;
}

VulkanPipeline::~VulkanPipeline() {
    Destroy();
}
