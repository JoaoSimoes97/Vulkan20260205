#pragma once

#include <memory>
#include <string>
#include <vector>

namespace tinygltf {
class Model;
class TinyGLTF;
}

/**
 * Loads glTF 2.0 files into a tinygltf::Model. Thread use: file I/O can be
 * offloaded via JobQueue (SubmitLoadFile); pass the received bytes to
 * LoadFromBytes() on the main thread to parse and create the in-memory model.
 * Resource creation (meshes, materials, textures) is done by managers on the
 * main thread when building the scene from the model.
 */
class GltfLoader {
public:
    GltfLoader();
    ~GltfLoader();

    /** Load from file path (reads on current thread). Returns true on success. */
    bool LoadFromFile(const std::string& path);

    /**
     * Load from pre-read bytes (e.g. from JobQueue completed job). Call from
     * main thread. Returns true on success. Previous model is replaced.
     */
    bool LoadFromBytes(const std::vector<uint8_t>& bytes, const std::string& basePathOrEmpty = "");

    /** Non-owning access to the loaded model. Invalid after next Load or Destroy. */
    const tinygltf::Model* GetModel() const;
    tinygltf::Model* GetModel();

    /** Write model to file (.glb or .gltf). Returns true on success. */
    bool WriteToFile(const tinygltf::Model& model, const std::string& path);

    void Clear();

private:
    std::unique_ptr<tinygltf::TinyGLTF> m_loader;
    std::unique_ptr<tinygltf::Model> m_model;
};
