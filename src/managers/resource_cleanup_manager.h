#pragma once

#include <memory>

class MaterialManager;
class MeshManager;
class TextureManager;
class PipelineManager;
class VulkanShaderManager;

/**
 * ResourceCleanupManager — centralized interface for trimming all manager caches.
 * Enqueued on worker thread to trim unused resources asynchronously.
 * Called once per frame by ResourceManagerThread.
 */
class ResourceCleanupManager {
public:
    ResourceCleanupManager() = default;

    /** Set pointers to all managers. Call after all managers created. */
    void SetManagers(
        MaterialManager* pMaterialManager,
        MeshManager* pMeshManager,
        TextureManager* pTextureManager,
        PipelineManager* pPipelineManager,
        VulkanShaderManager* pShaderManager
    );

    /** Trim all manager caches (removes unreferenced resources). */
    void TrimAllCaches();

    /** Trim specific manager cache. */
    void TrimMaterials();
    void TrimMeshes();
    void TrimTextures();
    void TrimPipelines();
    void TrimShaders();

    /** Enable/disable trimming per manager. */
    void SetTrimMaterial(bool enable) { m_bTrimMaterial = enable; }
    void SetTrimMesh(bool enable) { m_bTrimMesh = enable; }
    void SetTrimTexture(bool enable) { m_bTrimTexture = enable; }
    void SetTrimPipeline(bool enable) { m_bTrimPipeline = enable; }
    void SetTrimShader(bool enable) { m_bTrimShader = enable; }

private:
    MaterialManager* m_pMaterialManager = nullptr;
    MeshManager* m_pMeshManager = nullptr;
    TextureManager* m_pTextureManager = nullptr;
    PipelineManager* m_pPipelineManager = nullptr;
    VulkanShaderManager* m_pShaderManager = nullptr;

    /* Per-manager trim control */
    bool m_bTrimMaterial = true;
    bool m_bTrimMesh = true;
    bool m_bTrimTexture = true;
    bool m_bTrimPipeline = true;
    bool m_bTrimShader = true;
};
