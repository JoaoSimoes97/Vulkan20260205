#include "resource_cleanup_manager.h"
#include "material_manager.h"
#include "mesh_manager.h"
#include "texture_manager.h"
#include "pipeline_manager.h"
#include "vulkan/vulkan_shader_manager.h"
#include "vulkan/vulkan_utils.h"

void ResourceCleanupManager::SetManagers(
    MaterialManager* pMaterialManager,
    MeshManager* pMeshManager,
    TextureManager* pTextureManager,
    PipelineManager* pPipelineManager,
    VulkanShaderManager* pShaderManager
) {
    m_pMaterialManager = pMaterialManager;
    m_pMeshManager = pMeshManager;
    m_pTextureManager = pTextureManager;
    m_pPipelineManager = pPipelineManager;
    m_pShaderManager = pShaderManager;

    VulkanUtils::LogDebug("ResourceCleanupManager: all managers registered");
}

void ResourceCleanupManager::TrimAllCaches() {
    if (m_bTrimMaterial && m_pMaterialManager)
        TrimMaterials();
    if (m_bTrimMesh && m_pMeshManager)
        TrimMeshes();
    if (m_bTrimTexture && m_pTextureManager)
        TrimTextures();
    if (m_bTrimPipeline && m_pPipelineManager)
        TrimPipelines();
    if (m_bTrimShader && m_pShaderManager)
        TrimShaders();
}

void ResourceCleanupManager::TrimMaterials() {
    if (m_pMaterialManager)
        m_pMaterialManager->TrimUnused();
}

void ResourceCleanupManager::TrimMeshes() {
    if (m_pMeshManager)
        m_pMeshManager->TrimUnused();
}

void ResourceCleanupManager::TrimTextures() {
    if (m_pTextureManager)
        m_pTextureManager->TrimUnused();
}

void ResourceCleanupManager::TrimPipelines() {
    if (m_pPipelineManager)
        m_pPipelineManager->TrimUnused();
}

void ResourceCleanupManager::TrimShaders() {
    if (m_pShaderManager)
        m_pShaderManager->TrimUnused();
}
