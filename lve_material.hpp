#pragma once
#include "lve_texture.hpp"
#include "lve_descriptors.hpp"
#include "lve_device.hpp"
#include "lve_buffer.hpp"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>

namespace lve
{

    // 材质参数 Uniform Buffer 结构体（必须与着色器中的布局匹配）
    struct alignas(16) MaterialUBO
    {
        glm::vec4 baseColorFactor{1.0f}; // offset 0
        float metallicFactor{0.0f};      // offset 16
        float roughnessFactor{0.5f};     // offset 20
        int hasDiffuseMap{0};            // offset 24
        int hasNormalMap{0};             // offset 28
        int hasSpecularMap{0};           // offset 32
        // 编译器自动填充到 48 字节（16的倍数）
    };

    static_assert(sizeof(MaterialUBO) % 16 == 0,
                  "MaterialUBO size must be multiple of 16 bytes");

    class LveMaterial
    {
    public:
        // 纹理类型枚举
        enum class TextureType
        {
            DIFFUSE,   // 漫反射贴图
            NORMAL,    // 法线贴图
            SPECULAR,  // 高光贴图
            EMISSIVE,  // 自发光贴图
            METALLIC,  // 金属度贴图
            ROUGHNESS, // 粗糙度贴图
            AO,        // 环境光遮蔽贴图
            CUSTOM     // 自定义
        };

        // 材质参数
        struct MaterialParameters
        {
            glm::vec4 baseColorFactor = glm::vec4(1.0f);
            float metallicFactor = 0.0f;
            float roughnessFactor = 0.5f;
            float emissiveFactor = 0.0f;
            float alphaCutoff = 0.5f;
            bool doubleSided = false;
        };

        LveMaterial(LveDescriptorSetLayout &layout, LveDescriptorPool &pool, LveDevice &device);
        ~LveMaterial();

        // 禁止拷贝
        LveMaterial(const LveMaterial &) = delete;
        LveMaterial &operator=(const LveMaterial &) = delete;

        // 允许移动
        LveMaterial(LveMaterial &&other) noexcept;
        LveMaterial &operator=(LveMaterial &&other) noexcept;

        // 设置纹理
        void setTexture(TextureType type, std::shared_ptr<LveTexture> texture);
        void setTexture(const std::string &name, std::shared_ptr<LveTexture> texture, uint32_t binding);

        // 批量设置纹理
        void setTextures(const std::unordered_map<TextureType, std::shared_ptr<LveTexture>> &textures);

        // 设置材质参数
        void setParameters(const MaterialParameters &params);
        void setBaseColorFactor(const glm::vec4 &color);
        void setMetallicFactor(float metallic);
        void setRoughnessFactor(float roughness);

        // 构建描述符集
        void buildDescriptorSet();

        // 更新已有的描述符集（当纹理改变时）
        void updateDescriptorSet();

        // 更新 Uniform Buffer
        void updateUniformBuffer();

        // 获取描述符集
        VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }

        // 获取纹理
        std::shared_ptr<LveTexture> getTexture(TextureType type) const;

        // 获取材质参数
        const MaterialParameters &getParameters() const { return m_parameters; }

        // 检查是否已构建
        bool isBuilt() const { return m_built; }

    private:
        // 获取纹理类型的默认绑定点
        uint32_t getDefaultBinding(TextureType type) const;

        // 准备描述符写入
        void prepareDescriptorWrites();

        LveDescriptorSetLayout &m_layout;
        LveDescriptorPool &m_pool;
        LveDevice &m_device;

        std::unordered_map<uint32_t, std::shared_ptr<LveTexture>> m_textures;
        std::unordered_map<TextureType, uint32_t> m_typeToBinding;
        std::vector<VkDescriptorImageInfo> m_imageInfos;

        MaterialParameters m_parameters;
        VkDescriptorSet m_descriptorSet;
        bool m_built;
        bool m_dirty;

        // 材质参数 UBO 相关成员
        std::unique_ptr<LveBuffer> m_materialBuffer;
        VkDescriptorBufferInfo m_materialBufferInfo{};
        MaterialUBO m_materialUBO{};
        bool m_uboDirty{true};
    };

    // 材质管理器
    class LveMaterialManager
    {
    public:
        LveMaterialManager(LveDescriptorSetLayout &layout, LveDescriptorPool &pool, LveDevice &device);

        std::shared_ptr<LveMaterial> createMaterial(const std::string &name);
        std::shared_ptr<LveMaterial> getMaterial(const std::string &name);
        void removeMaterial(const std::string &name);
        void buildAllMaterials();

    private:
        LveDescriptorSetLayout &m_layout;
        LveDescriptorPool &m_pool;
        LveDevice &m_device;
        std::unordered_map<std::string, std::shared_ptr<LveMaterial>> m_materials;
    };

} // namespace lve