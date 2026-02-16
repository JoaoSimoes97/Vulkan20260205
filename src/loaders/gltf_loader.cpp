/*
 * GltfLoader — load glTF 2.0 via TinyGLTF. LoadFromBytes() is intended to be
 * called on the main thread with data from a JobQueue file read (I/O offloaded).
 */
#include "gltf_loader.h"
#include "vulkan/vulkan_utils.h"
#include <tiny_gltf.h>
#include <cstring>
#include <fstream>

// Forward declarations for stb_image functions (implementation in texture_manager.cpp)
extern "C" {
    unsigned char *stbi_load_from_memory(unsigned char const *buffer, int len, int *x, int *y, int *channels_in_file, int desired_channels);
    void stbi_image_free(void *retval_from_stbi_load);
}

namespace {

const unsigned char kGlbMagic[] = { 'g', 'l', 'T', 'F' };
constexpr size_t kGlbMagicLen = 4u;

bool IsGlb(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < kGlbMagicLen)
        return false;
    return std::memcmp(bytes.data(), kGlbMagic, kGlbMagicLen) == 0;
}

/**
 * Custom image loader for TinyGLTF using stb_image.
 * Required because TINYGLTF_NO_STB_IMAGE is defined to avoid conflicts with texture_manager.
 */
bool CustomLoadImageData(tinygltf::Image *image, const int image_idx, std::string *err,
                         std::string *warn, int req_width, int req_height,
                         const unsigned char *bytes, int size, void *user_data) {
    (void)warn;
    (void)user_data;
    (void)image_idx;
    (void)req_width;
    (void)req_height;
    
    int w = 0, h = 0, comp = 0;
    unsigned char *data = stbi_load_from_memory(bytes, size, &w, &h, &comp, 0);
    if (!data) {
        if (err) {
            (*err) += "Failed to load image with stb_image.\n";
        }
        return false;
    }
    
    image->width = w;
    image->height = h;
    image->component = comp;
    image->image.resize(static_cast<size_t>(w * h * comp));
    std::memcpy(&image->image[0], data, image->image.size());
    stbi_image_free(data);
    
    return true;
}

} // namespace

GltfLoader::GltfLoader() {
    m_loader = std::make_unique<tinygltf::TinyGLTF>();
    m_model = std::make_unique<tinygltf::Model>();
    
    // Set custom image loader (TINYGLTF_NO_STB_IMAGE is defined, so we need to provide our own)
    m_loader->SetImageLoader(CustomLoadImageData, nullptr);
}

GltfLoader::~GltfLoader() = default;

bool GltfLoader::LoadFromFile(const std::string& path) {
    Clear();
    m_model = std::make_unique<tinygltf::Model>();
    std::string err;
    std::string warn;
    bool ok = false;
    if (path.size() >= 4u && path.compare(path.size() - 4, 4, ".glb") == 0)
        ok = m_loader->LoadBinaryFromFile(m_model.get(), &err, &warn, path);
    else
        ok = m_loader->LoadASCIIFromFile(m_model.get(), &err, &warn, path);
    if (!warn.empty())
        VulkanUtils::LogDebug("GltfLoader: {}", warn);
    if (!ok && !err.empty())
        VulkanUtils::LogErr("GltfLoader: {}", err);
    return ok;
}

bool GltfLoader::LoadFromBytes(const std::vector<uint8_t>& bytes, const std::string& basePathOrEmpty) {
    Clear();
    m_model = std::make_unique<tinygltf::Model>();
    if (bytes.empty())
        return false;
    std::string err;
    std::string warn;
    bool ok = false;
    const size_t len = bytes.size();
    if (IsGlb(bytes)) {
        ok = m_loader->LoadBinaryFromMemory(m_model.get(), &err, &warn,
                                            bytes.data(), static_cast<unsigned int>(len), basePathOrEmpty);
    } else {
        const char* str = reinterpret_cast<const char*>(bytes.data());
        ok = m_loader->LoadASCIIFromString(m_model.get(), &err, &warn,
                                            str, static_cast<unsigned int>(len), basePathOrEmpty);
    }
    if (!warn.empty())
        VulkanUtils::LogDebug("GltfLoader: {}", warn);
    if (!ok && !err.empty())
        VulkanUtils::LogErr("GltfLoader: {}", err);
    return ok;
}

const tinygltf::Model* GltfLoader::GetModel() const {
    return m_model.get();
}

tinygltf::Model* GltfLoader::GetModel() {
    return m_model.get();
}

bool GltfLoader::WriteToFile(const tinygltf::Model& model, const std::string& path) {
    if (m_loader == nullptr || path.empty())
        return false;
    const bool writeBinary = (path.size() >= 4u && path.compare(path.size() - 4, 4, ".glb") == 0);
    return m_loader->WriteGltfSceneToFile(&model, path, false, true, true, writeBinary);
}

void GltfLoader::Clear() {
    m_model.reset();
    m_model = std::make_unique<tinygltf::Model>();
}
