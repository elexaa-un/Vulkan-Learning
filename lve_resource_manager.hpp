// Vulkan学习项目 — 统一资源管理器
// 管理模型、纹理、材质、描述符集布局和描述符池的统一缓存和生命周期

#pragma once
#include "lve_device.hpp"
#include "lve_model.hpp"
#include "lve_texture.hpp"
#include "lve_material.hpp"
#include "lve_descriptors.hpp"
#include "lve_buffer.hpp"
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <vector>

namespace lve
{

    // 统一资源管理器：提供模型、纹理、材质、描述符布局/池的集中式管理
    // 通过缓存避免重复加载，支持引用计数驱逐未使用资源
    class LveResourceManager
    {
    public:
        // 构造函数：传入Vulkan设备引用
        explicit LveResourceManager(LveDevice &device);
        ~LveResourceManager();

        // 禁止拷贝和移动
        LveResourceManager(const LveResourceManager &) = delete;
        LveResourceManager &operator=(const LveResourceManager &) = delete;

        // ============ 模型管理 ============
        // 从文件加载模型（若已缓存则直接返回）
        std::shared_ptr<LveModel> loadModel(const std::string &filepath);

        // 从顶点/索引数据创建模型（不缓存，或按自定义 key 缓存）
        std::shared_ptr<LveModel> createModelFromData(
            const std::string &cacheKey,
            const std::vector<LveModel::Vertex> &vertices,
            const std::vector<uint32_t> &indices);

        // 获取已缓存的模型（不会触发加载）
        std::shared_ptr<LveModel> getModel(const std::string &filepath) const;

        // 检查是否已缓存
        bool hasModel(const std::string &filepath) const;

        // 驱逐指定模型
        void evictModel(const std::string &filepath);

        // 驱逐所有未使用的模型（引用计数 == 1，即仅缓存持有）
        size_t evictUnusedModels();

        // ============ 纹理管理 ============
        // 加载 2D 纹理
        std::shared_ptr<LveTexture> loadTexture(const std::string &filepath);

        // 创建空纹理（不缓存，或按 key 缓存）
        std::shared_ptr<LveTexture> createEmptyTexture(
            const std::string &cacheKey,
            uint32_t width, uint32_t height, VkFormat format);

        // 加载立方体贴图（6 个面）
        std::shared_ptr<LveTexture> loadCubemap(
            const std::string &cacheKey,
            const std::array<std::string, 6> &facePaths);

        // 从等距矩形图创建立方体贴图
        std::shared_ptr<LveTexture> loadEquirectangularCubemap(
            const std::string &equirectangularPath);

        // 获取已缓存纹理
        std::shared_ptr<LveTexture> getTexture(const std::string &key) const;
        bool hasTexture(const std::string &key) const;
        void evictTexture(const std::string &key);
        size_t evictUnusedTextures();

        // ============ 材质管理 (整合现有 LveMaterialManager) ============
        // 需要先设置材质专用的 layout 和 pool
        void setMaterialDescriptorInfo(
            LveDescriptorSetLayout &layout,
            LveDescriptorPool &pool);

        std::shared_ptr<LveMaterial> createMaterial(const std::string &name);
        std::shared_ptr<LveMaterial> getMaterial(const std::string &name) const;
        void removeMaterial(const std::string &name);
        void buildAllMaterials();
        bool hasMaterial(const std::string &name) const;

        // ============ DescriptorSetLayout 缓存 ============
        // 根据 Builder 配置自动去重：相同绑定描述的 layout 只创建一次
        // 返回 shared_ptr，便于多处共享同一 layout
        std::shared_ptr<LveDescriptorSetLayout> getOrCreateDescriptorSetLayout(
            const LveDescriptorSetLayout::Builder &builder);

        // ============ DescriptorPool 管理 ============
        // 创建并托管 descriptor pool
        LveDescriptorPool &createDescriptorPool(
            const std::string &poolName,
            const LveDescriptorPool::Builder &builder);

        LveDescriptorPool *getDescriptorPool(const std::string &poolName) const;

        // ============ Buffer 池（可选） ============
        // 创建 UBO buffer（可用于帧资源等场景）
        std::shared_ptr<LveBuffer> createUniformBuffer(
            VkDeviceSize size,
            VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        // ============ 全局操作 ============
        // 驱逐所有仅被缓存持有的资源
        size_t evictAllUnused();

        // 清空所有缓存（谨慎使用，确保外部无引用）
        void clearAll();

        // 获取缓存统计信息
        struct CacheStats
        {
            size_t modelCount;
            size_t textureCount;
            size_t cubemapCount;
            size_t materialCount;
            size_t descriptorLayoutCount;
            size_t totalGpuMemoryEstimate; // 估算的 GPU 内存占用
        };
        CacheStats getStats() const;

    private:
        LveDevice &m_device;

        // 模型缓存
        std::unordered_map<std::string, std::shared_ptr<LveModel>> m_modelCache;

        // 纹理缓存（2D 纹理 + 立方体贴图共用）
        std::unordered_map<std::string, std::shared_ptr<LveTexture>> m_textureCache;

        // 材质管理器内部实现
        LveDescriptorSetLayout *m_materialLayout = nullptr;
        LveDescriptorPool *m_materialPool = nullptr;
        std::unordered_map<std::string, std::shared_ptr<LveMaterial>> m_materialCache;

        // DescriptorSetLayout 缓存：key = 绑定描述的哈希值
        std::unordered_map<size_t, std::shared_ptr<LveDescriptorSetLayout>> m_layoutCache;

        // DescriptorPool 集合
        std::unordered_map<std::string, std::unique_ptr<LveDescriptorPool>> m_poolCache;

        // 辅助：计算 Builder 的哈希（用于 layout 去重）
        static size_t hashDescriptorLayoutBuilder(const LveDescriptorSetLayout::Builder &builder);
    };

} // namespace lve
