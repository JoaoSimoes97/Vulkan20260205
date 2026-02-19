#pragma once

#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <shared_mutex>
#include <vulkan/vulkan.h>

struct JobQueue;

/**
 * Texture handle: owns VkImage, VkImageView, VkSampler, VkDeviceMemory. Destructor frees GPU resources.
 */
class TextureHandle {
public:
    TextureHandle() = default;
    ~TextureHandle();

    TextureHandle(const TextureHandle&) = delete;
    TextureHandle& operator=(const TextureHandle&) = delete;
    TextureHandle(TextureHandle&& other) noexcept;
    TextureHandle& operator=(TextureHandle&& other) noexcept;

    void Set(VkDevice device, VkImage image, VkImageView view, VkSampler sampler, VkDeviceMemory memory);

    VkImageView GetView() const { return m_view; }
    VkSampler GetSampler() const { return m_sampler; }
    bool IsValid() const { return m_view != VK_NULL_HANDLE && m_sampler != VK_NULL_HANDLE; }

private:
    void Destroy();

    VkDevice       m_device = VK_NULL_HANDLE;
    VkImage        m_image  = VK_NULL_HANDLE;
    VkImageView    m_view   = VK_NULL_HANDLE;
    VkSampler      m_sampler = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
};

/**
 * Get-or-load textures by path. Async load via RequestLoadTexture + OnCompletedTexture (from job queue).
 * SetDevice/SetPhysicalDevice/SetQueue/SetQueueFamilyIndex before use. SetJobQueue before RequestLoadTexture.
 * Destroy() clears cache (call before device destroy).
 */
class TextureManager {
public:
    TextureManager() = default;

    void SetJobQueue(JobQueue* pJobQueue);
    void SetDevice(VkDevice device);
    void SetPhysicalDevice(VkPhysicalDevice physicalDevice);
    void SetQueue(VkQueue queue);
    void SetQueueFamilyIndex(uint32_t queueFamilyIndex);

    /** Return cached texture or nullptr if not loaded yet. */
    std::shared_ptr<TextureHandle> GetTexture(const std::string& path) const;
    /** Create and cache a 1x1 white texture for default/fallback (e.g. descriptor set). Returns nullptr if device not set. */
    std::shared_ptr<TextureHandle> GetOrCreateDefaultTexture();
    /** Create and cache texture from memory (e.g. glTF embedded image). Cache key = cacheKey param. */
    std::shared_ptr<TextureHandle> GetOrCreateFromMemory(const std::string& cacheKey, int width, int height, int channels, const unsigned char* pPixels);
    void RequestLoadTexture(const std::string& path);
    void OnCompletedTexture(const std::string& sPath_ic, std::vector<uint8_t> vecData_in);

    void TrimUnused();
    void Destroy();

private:
    std::shared_ptr<TextureHandle> UploadTexture(int width, int height, int channels, const unsigned char* pPixels);

    JobQueue* m_pJobQueue = nullptr;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    uint32_t m_queueFamilyIndex = 0u;
    mutable std::shared_mutex m_mutex;
    std::map<std::string, std::shared_ptr<TextureHandle>> m_cache;
    std::set<std::string> m_pendingPaths;
};
