/*
 * GltfLoader — load glTF 2.0 via TinyGLTF. LoadFromBytes() is intended to be
 * called on the main thread with data from a JobQueue file read (I/O offloaded).
 */
#include "gltf_loader.h"
#include "vulkan/vulkan_utils.h"
#include <tiny_gltf.h>
#include <cstring>
#include <fstream>

namespace {

const unsigned char kGlbMagic[] = { 'g', 'l', 'T', 'F' };
constexpr size_t kGlbMagicLen = 4u;

bool IsGlb(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < kGlbMagicLen)
        return false;
    return std::memcmp(bytes.data(), kGlbMagic, kGlbMagicLen) == 0;
}

} // namespace

GltfLoader::GltfLoader() {
    m_loader = std::make_unique<tinygltf::TinyGLTF>();
    m_model = std::make_unique<tinygltf::Model>();
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
