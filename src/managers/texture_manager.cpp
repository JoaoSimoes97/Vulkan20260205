/*
 * TextureManager — load images (stb_image), upload to VkImage, cache by path. Async via JobQueue.
 */
#define STB_IMAGE_IMPLEMENTATION
#include "texture_manager.h"
#include "thread/job_queue.h"
#include "vulkan/vulkan_utils.h"
#include <stb_image.h>
#include <cstring>
#include <stdexcept>

namespace {

uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("TextureManager: no suitable memory type");
}

VkCommandBuffer BeginSingleTimeCommands(VkDevice device, VkCommandPool pool) {
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &allocInfo, &cmd) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        return VK_NULL_HANDLE;
    }
    return cmd;
}

void EndSingleTimeCommands(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr,
    };
    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, pool, 1, &cmd);
}

void TransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = static_cast<VkAccessFlags>(0),
        .dstAccessMask = static_cast<VkAccessFlags>(0),
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        return;
    }
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

} // namespace

// -----------------------------------------------------------------------------
// TextureHandle
// -----------------------------------------------------------------------------
TextureHandle::~TextureHandle() {
    Destroy();
}

TextureHandle::TextureHandle(TextureHandle&& other) noexcept
    : m_device(other.m_device)
    , m_image(other.m_image)
    , m_view(other.m_view)
    , m_sampler(other.m_sampler)
    , m_memory(other.m_memory) {
    other.m_device = VK_NULL_HANDLE;
    other.m_image = VK_NULL_HANDLE;
    other.m_view = VK_NULL_HANDLE;
    other.m_sampler = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
}

TextureHandle& TextureHandle::operator=(TextureHandle&& other) noexcept {
    if (this == &other) return *this;
    Destroy();
    m_device = other.m_device;
    m_image = other.m_image;
    m_view = other.m_view;
    m_sampler = other.m_sampler;
    m_memory = other.m_memory;
    other.m_device = VK_NULL_HANDLE;
    other.m_image = VK_NULL_HANDLE;
    other.m_view = VK_NULL_HANDLE;
    other.m_sampler = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
    return *this;
}

void TextureHandle::Set(VkDevice device, VkImage image, VkImageView view, VkSampler sampler, VkDeviceMemory memory) {
    Destroy();
    m_device = device;
    m_image = image;
    m_view = view;
    m_sampler = sampler;
    m_memory = memory;
}

void TextureHandle::Destroy() {
    if (m_device == VK_NULL_HANDLE) return;
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_view, nullptr);
        m_view = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
    m_device = VK_NULL_HANDLE;
}

// -----------------------------------------------------------------------------
// TextureManager
// -----------------------------------------------------------------------------
void TextureManager::SetJobQueue(JobQueue* pJobQueue) {
    m_pJobQueue = pJobQueue;
}

void TextureManager::SetDevice(VkDevice device) {
    m_device = device;
}

void TextureManager::SetPhysicalDevice(VkPhysicalDevice physicalDevice) {
    m_physicalDevice = physicalDevice;
}

void TextureManager::SetQueue(VkQueue queue) {
    m_queue = queue;
}

void TextureManager::SetQueueFamilyIndex(uint32_t queueFamilyIndex) {
    m_queueFamilyIndex = queueFamilyIndex;
}

std::shared_ptr<TextureHandle> TextureManager::GetTexture(const std::string& path) const {
    auto it = m_cache.find(path);
    if (it == m_cache.end()) return nullptr;
    return it->second;
}

void TextureManager::RequestLoadTexture(const std::string& path) {
    if (m_pJobQueue == nullptr) return;
    if (m_pendingPaths.count(path) != 0) return;
    if (m_cache.count(path) != 0) return;
    m_pendingPaths.insert(path);
    m_pJobQueue->SubmitLoadTexture(path);
}

void TextureManager::OnCompletedTexture(const std::string& sPath_ic, std::vector<uint8_t> vecData_in) {
    if (this->m_pendingPaths.erase(sPath_ic) == 0)
        return;
    int iWidth = 0;
    int iHeight = 0;
    int iChannels = 0;
    unsigned char* pPixels = stbi_load_from_memory(
        vecData_in.data(), static_cast<int>(vecData_in.size()), &iWidth, &iHeight, &iChannels, 4);
    if ((pPixels == nullptr) || (iWidth <= 0) || (iHeight <= 0)) {
        VulkanUtils::LogErr("TextureManager: failed to decode {}", sPath_ic);
        return;
    }
    std::shared_ptr<TextureHandle> pHandle = UploadTexture(iWidth, iHeight, 4, pPixels);
    stbi_image_free(pPixels);
    if (pHandle != nullptr) {
        this->m_cache[sPath_ic] = pHandle;
        VulkanUtils::LogInfo("TextureManager: loaded {} ({}x{})", sPath_ic, iWidth, iHeight);
    }
}

std::shared_ptr<TextureHandle> TextureManager::UploadTexture(int width, int height, int channels, const unsigned char* pPixels) {
    if (m_device == VK_NULL_HANDLE || m_physicalDevice == VK_NULL_HANDLE || m_queue == VK_NULL_HANDLE ||
        pPixels == nullptr || width <= 0 || height <= 0)
        return nullptr;

    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * static_cast<VkDeviceSize>(channels);
    const VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    {
        VkBufferCreateInfo bufInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = static_cast<VkBufferCreateFlags>(0),
            .size = imageSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
        };
        if (vkCreateBuffer(m_device, &bufInfo, nullptr, &stagingBuffer) != VK_SUCCESS)
            return nullptr;
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memReqs);
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = FindMemoryType(m_physicalDevice, memReqs.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        };
        if (vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            return nullptr;
        }
        vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0);
        void* pMapped = nullptr;
        vkMapMemory(m_device, stagingMemory, 0, imageSize, 0, &pMapped);
        if (pMapped) {
            std::memcpy(pMapped, pPixels, static_cast<size_t>(imageSize));
            vkUnmapMemory(m_device, stagingMemory);
        }
    }

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    {
        VkImageCreateInfo imageInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = static_cast<VkImageCreateFlags>(0),
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        if (vkCreateImage(m_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            vkFreeMemory(m_device, stagingMemory, nullptr);
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            return nullptr;
        }
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(m_device, image, &memReqs);
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = FindMemoryType(m_physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        };
        if (vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            vkDestroyImage(m_device, image, nullptr);
            vkFreeMemory(m_device, stagingMemory, nullptr);
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            return nullptr;
        }
        vkBindImageMemory(m_device, image, imageMemory, 0);
    }

    VkCommandPool cmdPool = VK_NULL_HANDLE;
    {
        VkCommandPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = m_queueFamilyIndex,
        };
        if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &cmdPool) != VK_SUCCESS) {
            vkFreeMemory(m_device, imageMemory, nullptr);
            vkDestroyImage(m_device, image, nullptr);
            vkFreeMemory(m_device, stagingMemory, nullptr);
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            return nullptr;
        }
        VkCommandBuffer cmd = BeginSingleTimeCommands(m_device, cmdPool);
        if (cmd == VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, cmdPool, nullptr);
            vkFreeMemory(m_device, imageMemory, nullptr);
            vkDestroyImage(m_device, image, nullptr);
            vkFreeMemory(m_device, stagingMemory, nullptr);
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            return nullptr;
        }
        TransitionImageLayout(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy region = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .imageOffset = { 0, 0, 0 },
            .imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 },
        };
        vkCmdCopyBufferToImage(cmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        TransitionImageLayout(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        EndSingleTimeCommands(m_device, m_queue, cmdPool, cmd);
        vkDestroyCommandPool(m_device, cmdPool, nullptr);
    }

    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);

    VkImageView view = VK_NULL_HANDLE;
    {
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = static_cast<VkImageViewCreateFlags>(0),
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        if (vkCreateImageView(m_device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
            vkFreeMemory(m_device, imageMemory, nullptr);
            vkDestroyImage(m_device, image, nullptr);
            return nullptr;
        }
    }

    VkSampler sampler = VK_NULL_HANDLE;
    {
        VkSamplerCreateInfo samplerInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = static_cast<VkSamplerCreateFlags>(0),
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias = 0.f,
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 1.f,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_NEVER,
            .minLod = 0.f,
            .maxLod = 0.f,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
        };
        if (vkCreateSampler(m_device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            vkDestroyImageView(m_device, view, nullptr);
            vkFreeMemory(m_device, imageMemory, nullptr);
            vkDestroyImage(m_device, image, nullptr);
            return nullptr;
        }
    }

    auto handle = std::make_shared<TextureHandle>();
    handle->Set(m_device, image, view, sampler, imageMemory);
    return handle;
}

void TextureManager::TrimUnused() {
    for (auto it = m_cache.begin(); it != m_cache.end(); ) {
        if (it->second.use_count() == 1)
            it = m_cache.erase(it);
        else
            ++it;
    }
}

void TextureManager::Destroy() {
    m_pendingPaths.clear();
    m_cache.clear();
    m_pJobQueue = nullptr;
}
