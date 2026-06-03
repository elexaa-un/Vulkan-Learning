// Vulkan学习项目 — 统一资源管理器实现
// 模型、纹理、材质、描述符布局/池的统一缓存和生命周期管理

#include "lve_resource_manager.hpp"
#include "lve_utils.hpp"
#include <cstdint>
#include <functional>

namespace lve
{
    LveResourceManager::LveResourceManager(LveDevice &device)
        : m_device(device)
    {
    }

    LveResourceManager::~LveResourceManager() = default;

    // ============ 模型管理 ============
    std::shared_ptr<LveModel> LveResourceManager::loadModel(const std::string &filepath)
    {
        auto it = m_modelCache.find(filepath);
        if (it != m_modelCache.end())
        {
            return it->second;
        }

        auto uniqueModel = LveModel::createModelFromFile(m_device, filepath);
        std::shared_ptr<LveModel> sharedModel = std::move(uniqueModel);
        m_modelCache[filepath] = sharedModel;
        return sharedModel;
    }

    std::shared_ptr<LveModel> LveResourceManager::createModelFromData(
        const std::string &cacheKey,
        const std::vector<LveModel::Vertex> &vertices,
        const std::vector<uint32_t> &indices)
    {
        auto it = m_modelCache.find(cacheKey);
        if (it != m_modelCache.end())
        {
            return it->second;
        }

        auto model = LveModel::createModelFromVertices(m_device, vertices, indices);
        std::shared_ptr<LveModel> sharedModel = std::move(model);
        m_modelCache[cacheKey] = sharedModel;
        return sharedModel;
    }

    std::shared_ptr<LveModel> LveResourceManager::getModel(const std::string &filepath) const
    {
        auto it = m_modelCache.find(filepath);
        if (it != m_modelCache.end())
        {
            return it->second;
        }
        return nullptr;
    }

    bool LveResourceManager::hasModel(const std::string &filepath) const
    {
        return m_modelCache.find(filepath) != m_modelCache.end();
    }

    void LveResourceManager::evictModel(const std::string &filepath)
    {
        m_modelCache.erase(filepath);
    }

    size_t LveResourceManager::evictUnusedModels()
    {
        size_t count = 0;
        for (auto it = m_modelCache.begin(); it != m_modelCache.end();)
        {
            if (it->second.use_count() == 1)
            {
                it = m_modelCache.erase(it);
                ++count;
            }
            else
            {
                ++it;
            }
        }
        return count;
    }

    // ============ 纹理管理 ============
    std::shared_ptr<LveTexture> LveResourceManager::loadTexture(const std::string &filepath)
    {
        auto it = m_textureCache.find(filepath);
        if (it != m_textureCache.end())
        {
            return it->second;
        }

        auto texture = std::make_shared<LveTexture>(m_device, filepath);
        m_textureCache[filepath] = texture;
        return texture;
    }

    std::shared_ptr<LveTexture> LveResourceManager::createEmptyTexture(
        const std::string &cacheKey,
        uint32_t width, uint32_t height, VkFormat format)
    {
        auto it = m_textureCache.find(cacheKey);
        if (it != m_textureCache.end())
        {
            return it->second;
        }

        auto texture = std::make_shared<LveTexture>(m_device);
        texture->createEmpty(width, height, format);
        m_textureCache[cacheKey] = texture;
        return texture;
    }

    std::shared_ptr<LveTexture> LveResourceManager::loadCubemap(
        const std::string &cacheKey,
        const std::array<std::string, 6> &facePaths)
    {
        auto it = m_textureCache.find(cacheKey);
        if (it != m_textureCache.end())
        {
            return it->second;
        }

        auto texture = LveTexture::createCubemap(m_device, facePaths);
        m_textureCache[cacheKey] = texture;
        return texture;
    }

    std::shared_ptr<LveTexture> LveResourceManager::loadEquirectangularCubemap(
        const std::string &equirectangularPath)
    {
        std::string cacheKey = "equirect_" + equirectangularPath;
        auto it = m_textureCache.find(cacheKey);
        if (it != m_textureCache.end())
        {
            return it->second;
        }

        auto texture = LveTexture::createCubemapFromEquirectangular(m_device, equirectangularPath);
        m_textureCache[cacheKey] = texture;
        return texture;
    }

    std::shared_ptr<LveTexture> LveResourceManager::getTexture(const std::string &key) const
    {
        auto it = m_textureCache.find(key);
        if (it != m_textureCache.end())
        {
            return it->second;
        }
        return nullptr;
    }

    bool LveResourceManager::hasTexture(const std::string &key) const
    {
        return m_textureCache.find(key) != m_textureCache.end();
    }

    void LveResourceManager::evictTexture(const std::string &key)
    {
        m_textureCache.erase(key);
    }

    size_t LveResourceManager::evictUnusedTextures()
    {
        size_t count = 0;
        for (auto it = m_textureCache.begin(); it != m_textureCache.end();)
        {
            if (it->second.use_count() == 1)
            {
                it = m_textureCache.erase(it);
                ++count;
            }
            else
            {
                ++it;
            }
        }
        return count;
    }

    // ============ 材质管理 ============
    void LveResourceManager::setMaterialDescriptorInfo(
        LveDescriptorSetLayout &layout,
        LveDescriptorPool &pool)
    {
        m_materialLayout = &layout;
        m_materialPool = &pool;
    }

    std::shared_ptr<LveMaterial> LveResourceManager::createMaterial(const std::string &name)
    {
        auto it = m_materialCache.find(name);
        if (it != m_materialCache.end())
        {
            return it->second;
        }

        if (!m_materialLayout || !m_materialPool)
        {
            return nullptr; // 未设置材质描述符信息
        }

        auto material = std::make_shared<LveMaterial>(*m_materialLayout, *m_materialPool, m_device);
        m_materialCache[name] = material;
        return material;
    }

    std::shared_ptr<LveMaterial> LveResourceManager::getMaterial(const std::string &name) const
    {
        auto it = m_materialCache.find(name);
        if (it != m_materialCache.end())
        {
            return it->second;
        }
        return nullptr;
    }

    void LveResourceManager::removeMaterial(const std::string &name)
    {
        m_materialCache.erase(name);
    }

    void LveResourceManager::buildAllMaterials()
    {
        for (auto &[name, material] : m_materialCache)
        {
            if (!material->isBuilt())
            {
                material->buildDescriptorSet();
            }
        }
    }

    bool LveResourceManager::hasMaterial(const std::string &name) const
    {
        return m_materialCache.find(name) != m_materialCache.end();
    }

    // ============ DescriptorSetLayout 缓存 ============
    size_t LveResourceManager::hashDescriptorLayoutBuilder(
        const LveDescriptorSetLayout::Builder &builder)
    {
        const auto &bindings = builder.getBindings();
        size_t hash = 0;
        for (const auto &[binding, layoutBinding] : bindings)
        {
            size_t h1 = std::hash<uint32_t>{}(binding);
            size_t h2 = std::hash<uint32_t>{}(static_cast<uint32_t>(layoutBinding.descriptorType));
            size_t h3 = std::hash<uint32_t>{}(static_cast<uint32_t>(layoutBinding.stageFlags));
            size_t h4 = std::hash<uint32_t>{}(layoutBinding.descriptorCount);
            // 组合哈希: 模仿 boost::hash_combine
            size_t bindingHash = h1;
            bindingHash ^= h2 + 0x9e3779b9 + (bindingHash << 6) + (bindingHash >> 2);
            bindingHash ^= h3 + 0x9e3779b9 + (bindingHash << 6) + (bindingHash >> 2);
            bindingHash ^= h4 + 0x9e3779b9 + (bindingHash << 6) + (bindingHash >> 2);
            hash ^= bindingHash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }

    std::shared_ptr<LveDescriptorSetLayout> LveResourceManager::getOrCreateDescriptorSetLayout(
        const LveDescriptorSetLayout::Builder &builder)
    {
        size_t hash = hashDescriptorLayoutBuilder(builder);

        auto it = m_layoutCache.find(hash);
        if (it != m_layoutCache.end())
        {
            return it->second;
        }

        auto layout = builder.build();
        std::shared_ptr<LveDescriptorSetLayout> sharedLayout = std::move(layout);
        m_layoutCache[hash] = sharedLayout;
        return sharedLayout;
    }

    // ============ DescriptorPool 管理 ============
    LveDescriptorPool &LveResourceManager::createDescriptorPool(
        const std::string &poolName,
        const LveDescriptorPool::Builder &builder)
    {
        auto pool = builder.build();
        auto &ref = *pool;
        m_poolCache[poolName] = std::move(pool);
        return ref;
    }

    LveDescriptorPool *LveResourceManager::getDescriptorPool(const std::string &poolName) const
    {
        auto it = m_poolCache.find(poolName);
        if (it != m_poolCache.end())
        {
            return it->second.get();
        }
        return nullptr;
    }

    // ============ Buffer 池 ============
    std::shared_ptr<LveBuffer> LveResourceManager::createUniformBuffer(
        VkDeviceSize size,
        VkMemoryPropertyFlags flags)
    {
        auto buffer = std::make_shared<LveBuffer>(
            m_device,
            size,
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            flags,
            m_device.properties.limits.minUniformBufferOffsetAlignment);
        return buffer;
    }

    // ============ 全局操作 ============
    size_t LveResourceManager::evictAllUnused()
    {
        size_t total = 0;
        total += evictUnusedModels();
        total += evictUnusedTextures();
        // 材质不自动驱逐，因为材质通常由外部持有
        return total;
    }

    void LveResourceManager::clearAll()
    {
        m_modelCache.clear();
        m_textureCache.clear();
        m_materialCache.clear();
        m_layoutCache.clear();
        m_poolCache.clear();
    }

    LveResourceManager::CacheStats LveResourceManager::getStats() const
    {
        CacheStats stats{};
        stats.modelCount = m_modelCache.size();
        stats.textureCount = m_textureCache.size();
        stats.cubemapCount = 0;
        stats.materialCount = m_materialCache.size();
        stats.descriptorLayoutCount = m_layoutCache.size();
        stats.totalGpuMemoryEstimate = 0; // 暂不估算
        return stats;
    }

} // namespace lve
