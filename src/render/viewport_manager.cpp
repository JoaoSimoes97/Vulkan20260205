/*
 * ViewportManager implementation.
 */

#include "viewport_manager.h"
#include "../camera/camera.h"
#include "../core/scene_new.h"
#include "../core/camera_component.h"
#include "../core/transform.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <algorithm>
#include <array>

#ifndef NDEBUG
#include <imgui_impl_vulkan.h>
#endif

ViewportManager::~ViewportManager() {
    Destroy();
}

void ViewportManager::Create(VkDevice device, VkPhysicalDevice physicalDevice, 
                              VkRenderPass renderPass, VkDescriptorPool imguiDescriptorPool,
                              VkFormat colorFormat, VkFormat depthFormat,
                              uint32_t initialWidth, uint32_t initialHeight) {
    m_device = device;
    m_physicalDevice = physicalDevice;
    m_renderPass = renderPass;
    m_imguiDescriptorPool = imguiDescriptorPool;
    m_colorFormat = colorFormat;
    m_depthFormat = depthFormat;
    
    // Create offscreen render pass for viewports (using same formats as main render pass for compatibility)
    CreateOffscreenRenderPass();
    
    // Create default main viewport (ID 0) - renders to offscreen target
    ViewportConfig mainConfig;
    mainConfig.id = 0;
    mainConfig.name = "Main Viewport";
    mainConfig.bIsMainViewport = true;
    mainConfig.bVisible = true;
    mainConfig.renderMode = ViewportRenderMode::Solid;
    mainConfig.cameraGameObjectId = UINT32_MAX;  // Use main camera
    
    Viewport mainViewport;
    mainViewport.config = mainConfig;
    
    // Create render target for main viewport
    CreateRenderTarget(mainViewport.renderTarget, initialWidth, initialHeight);
    
    m_viewports.push_back(mainViewport);
}

void ViewportManager::Destroy() {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    
    vkDeviceWaitIdle(m_device);
    
    for (auto& viewport : m_viewports) {
        DestroyRenderTarget(viewport.renderTarget);
    }
    m_viewports.clear();
    
    if (m_offscreenRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
        m_offscreenRenderPass = VK_NULL_HANDLE;
    }
    
    m_device = VK_NULL_HANDLE;
}

uint32_t ViewportManager::AddViewport(const ViewportConfig& config) {
    Viewport viewport;
    viewport.config = config;
    viewport.config.id = m_nextId++;
    
    // Create render target if this is a PIP or detached viewport
    if (!viewport.config.bIsMainViewport) {
        CreateRenderTarget(viewport.renderTarget, 
                          static_cast<uint32_t>(viewport.config.pipSize.x),
                          static_cast<uint32_t>(viewport.config.pipSize.y));
    }
    
    m_viewports.push_back(viewport);
    return viewport.config.id;
}

void ViewportManager::RemoveViewport(uint32_t id) {
    if (id == 0) {
        // Cannot remove main viewport
        return;
    }
    
    auto it = std::find_if(m_viewports.begin(), m_viewports.end(),
                           [id](const Viewport& v) { return v.config.id == id; });
    
    if (it != m_viewports.end()) {
        DestroyRenderTarget(it->renderTarget);
        m_viewports.erase(it);
    }
}

Viewport* ViewportManager::GetViewport(uint32_t id) {
    for (auto& viewport : m_viewports) {
        if (viewport.config.id == id) {
            return &viewport;
        }
    }
    return nullptr;
}

const Viewport* ViewportManager::GetViewport(uint32_t id) const {
    for (const auto& viewport : m_viewports) {
        if (viewport.config.id == id) {
            return &viewport;
        }
    }
    return nullptr;
}

Viewport* ViewportManager::GetMainViewport() {
    return GetViewport(0);
}

VkDescriptorSet ViewportManager::GetMainViewportTextureId() const {
    const Viewport* pMainVp = GetViewport(0);
    if (pMainVp && pMainVp->renderTarget.IsValid()) {
        return pMainVp->renderTarget.imguiTextureId;
    }
    return VK_NULL_HANDLE;
}

void ViewportManager::GetMainViewportSize(uint32_t& outWidth, uint32_t& outHeight) const {
    const Viewport* pMainVp = GetViewport(0);
    if (pMainVp && pMainVp->renderTarget.IsValid()) {
        outWidth = pMainVp->renderTarget.width;
        outHeight = pMainVp->renderTarget.height;
    } else {
        outWidth = 0;
        outHeight = 0;
    }
}

void ViewportManager::ResizeViewport(uint32_t id, uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }
    
    Viewport* pViewport = GetViewport(id);
    if (!pViewport) {
        return;
    }
    
    auto& target = pViewport->renderTarget;
    if (target.width == width && target.height == height) {
        return;  // No resize needed
    }
    
    vkDeviceWaitIdle(m_device);
    DestroyRenderTarget(target);
    CreateRenderTarget(target, width, height);
    
    if (!pViewport->config.bIsMainViewport) {
        pViewport->config.pipSize = {static_cast<float>(width), static_cast<float>(height)};
    }
}

void ViewportManager::BeginViewportRender(uint32_t id, VkCommandBuffer cmd) {
    Viewport* pViewport = GetViewport(id);
    if (!pViewport) {
        return;
    }
    
    auto& target = pViewport->renderTarget;
    if (!target.IsValid()) {
        return;
    }
    
    // Begin render pass for this viewport
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_offscreenRenderPass;
    renderPassInfo.framebuffer = target.framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {target.width, target.height};
    
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{
        pViewport->config.clearColor.r,
        pViewport->config.clearColor.g,
        pViewport->config.clearColor.b,
        pViewport->config.clearColor.a
    }};
    clearValues[1].depthStencil = {1.0f, 0};
    
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Set viewport and scissor
    VkViewport vkViewport{};
    vkViewport.x = 0.0f;
    vkViewport.y = 0.0f;
    vkViewport.width = static_cast<float>(target.width);
    vkViewport.height = static_cast<float>(target.height);
    vkViewport.minDepth = 0.0f;
    vkViewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vkViewport);
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {target.width, target.height};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void ViewportManager::EndViewportRender(uint32_t id, VkCommandBuffer cmd) {
    Viewport* pViewport = GetViewport(id);
    if (!pViewport) {
        return;
    }
    
    auto& target = pViewport->renderTarget;
    if (!target.IsValid()) {
        return;
    }
    
    vkCmdEndRenderPass(cmd);
    
    // Transition color image to SHADER_READ_ONLY_OPTIMAL for ImGui sampling
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = target.colorImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

Camera* ViewportManager::GetCameraForViewport(const Viewport& viewport, SceneNew* pScene, Camera* pMainCamera) {
    // If no camera specified or no scene, use main camera
    if (viewport.config.cameraGameObjectId == UINT32_MAX || pScene == nullptr) {
        return pMainCamera;
    }
    
    // Look up the GameObject with this camera
    const GameObject* pGO = pScene->FindGameObject(viewport.config.cameraGameObjectId);
    if (pGO == nullptr || !pGO->HasCamera()) {
        return pMainCamera;
    }
    
    // Get or create cached camera
    uint32_t goId = viewport.config.cameraGameObjectId;
    auto it = m_cameraCache.find(goId);
    if (it == m_cameraCache.end()) {
        auto newCam = std::make_unique<Camera>();
        it = m_cameraCache.emplace(goId, std::move(newCam)).first;
    }
    
    Camera* pCam = it->second.get();
    
    // Get transform and camera component
    const Transform* pTransform = pScene->GetTransform(goId);
    const auto& cameras = pScene->GetCameras();
    const CameraComponent& camComp = cameras[pGO->cameraIndex];
    
    // Update camera position/rotation from transform
    if (pTransform != nullptr) {
        pCam->SetPosition(pTransform->position[0], pTransform->position[1], pTransform->position[2]);
        // Convert quaternion to yaw/pitch
        // Forward direction from transform gives us the look direction
        float fwd[3];
        TransformGetForward(*pTransform, fwd[0], fwd[1], fwd[2]);
        // Compute yaw (rotation around Y) and pitch (rotation around X) from forward
        float yaw = std::atan2(-fwd[0], -fwd[2]);  // -Z is forward, so negate
        float pitch = std::asin(fwd[1]);  // Up component gives pitch
        pCam->SetRotation(yaw, pitch);
    }
    
    // Build projection matrix from camera component
    glm::mat4 proj;
    float aspect = (camComp.aspectRatio > 0.f) ? camComp.aspectRatio : 
                   (viewport.renderTarget.width > 0 ? 
                    static_cast<float>(viewport.renderTarget.width) / static_cast<float>(viewport.renderTarget.height) : 1.f);
    
    if (camComp.projection == ProjectionType::Perspective) {
        proj = glm::perspective(camComp.fov, aspect, camComp.nearClip, camComp.farClip);
        proj[1][1] *= -1.f;  // Vulkan Y flip
    } else {
        float h = camComp.orthoSize;
        float w = h * aspect;
        proj = glm::ortho(-w, w, -h, h, camComp.nearClip, camComp.farClip);
        proj[1][1] *= -1.f;  // Vulkan Y flip
    }
    pCam->SetProjectionMatrix(proj);
    
    return pCam;
}

void ViewportManager::CreateRenderTarget(ViewportRenderTarget& target, uint32_t width, uint32_t height) {
    if (width == 0 || height == 0 || m_device == VK_NULL_HANDLE) {
        return;
    }
    
    target.width = width;
    target.height = height;
    
    // Create color image - use same format as main render pass
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_colorFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    
    if (vkCreateImage(m_device, &imageInfo, nullptr, &target.colorImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create viewport color image");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, target.colorImage, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &target.colorMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate viewport color memory");
    }
    
    vkBindImageMemory(m_device, target.colorImage, target.colorMemory, 0);
    
    // Create color image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = target.colorImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_colorFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &target.colorView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create viewport color image view");
    }
    
    // Create depth image - use same format as main render pass
    imageInfo.format = m_depthFormat;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    
    if (vkCreateImage(m_device, &imageInfo, nullptr, &target.depthImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create viewport depth image");
    }
    
    vkGetImageMemoryRequirements(m_device, target.depthImage, &memRequirements);
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &target.depthMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate viewport depth memory");
    }
    
    vkBindImageMemory(m_device, target.depthImage, target.depthMemory, 0);
    
    // Create depth image view
    viewInfo.image = target.depthImage;
    viewInfo.format = m_depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &target.depthView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create viewport depth image view");
    }
    
    // Create framebuffer
    std::array<VkImageView, 2> attachments = {target.colorView, target.depthView};
    
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_offscreenRenderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;
    
    if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &target.framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create viewport framebuffer");
    }
    
    // Create sampler for ImGui display
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &target.sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create viewport sampler");
    }
    
    // Register with ImGui for display (Debug builds only)
#ifndef NDEBUG
    target.imguiTextureId = ImGui_ImplVulkan_AddTexture(
        target.sampler, 
        target.colorView, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
#endif
}

void ViewportManager::DestroyRenderTarget(ViewportRenderTarget& target) {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    
    // Remove ImGui texture first (Debug builds only)
#ifndef NDEBUG
    if (target.imguiTextureId != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(target.imguiTextureId);
        target.imguiTextureId = VK_NULL_HANDLE;
    }
#endif
    
    if (target.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, target.sampler, nullptr);
        target.sampler = VK_NULL_HANDLE;
    }
    
    if (target.framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_device, target.framebuffer, nullptr);
        target.framebuffer = VK_NULL_HANDLE;
    }
    
    if (target.depthView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, target.depthView, nullptr);
        target.depthView = VK_NULL_HANDLE;
    }
    
    if (target.depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, target.depthImage, nullptr);
        target.depthImage = VK_NULL_HANDLE;
    }
    
    if (target.depthMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, target.depthMemory, nullptr);
        target.depthMemory = VK_NULL_HANDLE;
    }
    
    if (target.colorView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, target.colorView, nullptr);
        target.colorView = VK_NULL_HANDLE;
    }
    
    if (target.colorImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, target.colorImage, nullptr);
        target.colorImage = VK_NULL_HANDLE;
    }
    
    if (target.colorMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, target.colorMemory, nullptr);
        target.colorMemory = VK_NULL_HANDLE;
    }
    
    target.width = 0;
    target.height = 0;
}

void ViewportManager::CreateOffscreenRenderPass() {
    // Color attachment - use same format as main render pass for pipeline compatibility
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_colorFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;  // Will transition manually

    // Depth attachment - use same format as main render pass
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // Subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies{};
    
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_offscreenRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create offscreen render pass");
    }
}

uint32_t ViewportManager::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type for viewport");
}
