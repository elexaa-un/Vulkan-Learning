#include "lve_material.hpp"
#include <iostream>
#include <algorithm>
#include "lve_buffer.hpp"

namespace lve
{

    // LveMaterial 实现
    LveMaterial::LveMaterial(LveDescriptorSetLayout &layout, LveDescriptorPool &pool, LveDevice &device)
        : m_layout(layout), m_pool(pool), m_device(device),
          m_descriptorSet(VK_NULL_HANDLE), m_built(false), m_dirty(true)
    {
        // 创建材质参数 UBO
        VkDeviceSize alignment = std::max(m_device.properties.limits.minUniformBufferOffsetAlignment,
                                          m_device.properties.limits.nonCoherentAtomSize);
        m_materialBuffer = std::make_unique<LveBuffer>(
            m_device,
            sizeof(MaterialUBO),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            alignment);
        m_materialBuffer->map();

        m_materialBufferInfo.buffer = m_materialBuffer->getBuffer();
        m_materialBufferInfo.offset = 0;
        m_materialBufferInfo.range = sizeof(MaterialUBO);

        // 初始化纹理类型到绑定点映射
        m_typeToBinding[TextureType::DIFFUSE] = 1;  // binding 1 给漫反射贴图
        m_typeToBinding[TextureType::NORMAL] = 2;   // binding 2 给法线贴图
        m_typeToBinding[TextureType::SPECULAR] = 3; // binding 3 给高光贴图
    }

    void LveMaterial::updateUniformBuffer()
    {
        if (!m_uboDirty)
            return;

        // 更新 UBO 数据
        m_materialUBO.baseColorFactor = m_parameters.baseColorFactor;
        m_materialUBO.metallicFactor = m_parameters.metallicFactor;
        m_materialUBO.roughnessFactor = m_parameters.roughnessFactor;
        m_materialUBO.hasDiffuseMap = getTexture(TextureType::DIFFUSE) ? 1 : 0;
        m_materialUBO.hasNormalMap = getTexture(TextureType::NORMAL) ? 1 : 0;
        m_materialUBO.hasSpecularMap = getTexture(TextureType::SPECULAR) ? 1 : 0;

        m_materialBuffer->writeToBuffer(&m_materialUBO);
        m_uboDirty = false;
    }

    LveMaterial::~LveMaterial()
    {
        // 描述符集由 pool 管理，不需要手动销毁
    }

    LveMaterial::LveMaterial(LveMaterial &&other) noexcept
        : m_layout(other.m_layout), m_pool(other.m_pool), m_device(other.m_device),
          m_textures(std::move(other.m_textures)),
          m_typeToBinding(std::move(other.m_typeToBinding)),
          m_imageInfos(std::move(other.m_imageInfos)),
          m_parameters(other.m_parameters),
          m_descriptorSet(other.m_descriptorSet),
          m_built(other.m_built),
          m_dirty(other.m_dirty),
          m_materialBuffer(std::move(other.m_materialBuffer)),
          m_materialBufferInfo(other.m_materialBufferInfo),
          m_materialUBO(other.m_materialUBO),
          m_uboDirty(other.m_uboDirty)
    {
        other.m_descriptorSet = VK_NULL_HANDLE;
        other.m_built = false;
        other.m_materialBufferInfo = VkDescriptorBufferInfo{};
    }

    LveMaterial &LveMaterial::operator=(LveMaterial &&other) noexcept
    {
        if (this != &other)
        {
            m_textures = std::move(other.m_textures);
            m_typeToBinding = std::move(other.m_typeToBinding);
            m_imageInfos = std::move(other.m_imageInfos);
            m_parameters = other.m_parameters;
            m_descriptorSet = other.m_descriptorSet;
            m_built = other.m_built;
            m_dirty = other.m_dirty;
            m_materialBuffer = std::move(other.m_materialBuffer);
            m_materialBufferInfo = other.m_materialBufferInfo;
            m_materialUBO = other.m_materialUBO;
            m_uboDirty = other.m_uboDirty;

            other.m_descriptorSet = VK_NULL_HANDLE;
            other.m_built = false;
            other.m_materialBufferInfo = VkDescriptorBufferInfo{};
        }
        return *this;
    }

    void LveMaterial::setTexture(TextureType type, std::shared_ptr<LveTexture> texture)
    {
        uint32_t binding = getDefaultBinding(type);
        m_textures[binding] = texture;
        m_dirty = true;
        m_uboDirty = true; // 纹理变化也需要更新 UBO 中的标志
    }

    void LveMaterial::setTexture(const std::string &name, std::shared_ptr<LveTexture> texture, uint32_t binding)
    {
        m_textures[binding] = texture;
        m_dirty = true;
        m_uboDirty = true;
    }

    void LveMaterial::setParameters(const MaterialParameters &params)
    {
        m_parameters = params;
        m_uboDirty = true;
        m_dirty = true;
    }

    void LveMaterial::setBaseColorFactor(const glm::vec4 &color)
    {
        m_parameters.baseColorFactor = color;
        m_uboDirty = true;
    }

    void LveMaterial::setMetallicFactor(float metallic)
    {
        m_parameters.metallicFactor = metallic;
        m_uboDirty = true;
    }

    void LveMaterial::setRoughnessFactor(float roughness)
    {
        m_parameters.roughnessFactor = roughness;
        m_uboDirty = true;
    }

    void LveMaterial::buildDescriptorSet()
    {
        updateUniformBuffer();

        if (!m_dirty && m_built)
        {
            return;
        }

        prepareDescriptorWrites();

        // 构建描述符集
        LveDescriptorWriter writer(m_layout, m_pool);

        // Binding 0: Uniform Buffer (材质参数)
        writer.writeBuffer(0, &m_materialBufferInfo);

        // Binding 1+: 纹理
        for (size_t i = 0; i < m_imageInfos.size(); i++)
        {
            // 获取实际的绑定点（从 sortedTextures 中）
            auto it = m_textures.begin();
            std::advance(it, i);
            uint32_t binding = it->first;
            writer.writeImage(binding, &m_imageInfos[i]);
        }

        writer.build(m_descriptorSet);
        m_built = true;
        m_dirty = false;

        std::cout << "Material built with " << m_imageInfos.size() << " textures" << std::endl;
    }

    void LveMaterial::updateDescriptorSet()
    {
        if (!m_built)
        {
            buildDescriptorSet();
            return;
        }

        updateUniformBuffer();
        prepareDescriptorWrites();

        // 更新已有的描述符集
        std::vector<VkWriteDescriptorSet> writes;

        // 更新 UBO
        VkWriteDescriptorSet uboWrite{};
        uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uboWrite.dstSet = m_descriptorSet;
        uboWrite.dstBinding = 0;
        uboWrite.dstArrayElement = 0;
        uboWrite.descriptorCount = 1;
        uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboWrite.pBufferInfo = &m_materialBufferInfo;
        writes.push_back(uboWrite);

        // 更新纹理
        for (size_t i = 0; i < m_imageInfos.size(); i++)
        {
            auto it = m_textures.begin();
            std::advance(it, i);

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_descriptorSet;
            write.dstBinding = it->first;
            write.dstArrayElement = 0;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo = &m_imageInfos[i];
            writes.push_back(write);
        }

        vkUpdateDescriptorSets(m_device.device(),
                               static_cast<uint32_t>(writes.size()),
                               writes.data(),
                               0, nullptr);
    }

    std::shared_ptr<LveTexture> LveMaterial::getTexture(TextureType type) const
    {
        uint32_t binding = getDefaultBinding(type);
        auto it = m_textures.find(binding);
        if (it != m_textures.end())
        {
            return it->second;
        }
        return nullptr;
    }

    uint32_t LveMaterial::getDefaultBinding(TextureType type) const
    {
        auto it = m_typeToBinding.find(type);
        if (it != m_typeToBinding.end())
        {
            return it->second;
        }
        return static_cast<uint32_t>(TextureType::CUSTOM);
    }

    void LveMaterial::prepareDescriptorWrites()
    {
        m_imageInfos.clear();

        // 按绑定点排序
        std::vector<std::pair<uint32_t, std::shared_ptr<LveTexture>>> sortedTextures(
            m_textures.begin(), m_textures.end());

        std::sort(sortedTextures.begin(), sortedTextures.end(),
                  [](const auto &a, const auto &b)
                  {
                      return a.first < b.first;
                  });

        for (const auto &[binding, texture] : sortedTextures)
        {
            if (!texture)
                continue;

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = texture->getImageView();
            imageInfo.sampler = texture->getSampler();
            m_imageInfos.push_back(imageInfo);
        }
    }

    // LveMaterialManager 实现
    LveMaterialManager::LveMaterialManager(LveDescriptorSetLayout &layout,
                                           LveDescriptorPool &pool,
                                           LveDevice &device)
        : m_layout(layout), m_pool(pool), m_device(device) {}

    std::shared_ptr<LveMaterial> LveMaterialManager::createMaterial(const std::string &name)
    {
        auto material = std::make_shared<LveMaterial>(m_layout, m_pool, m_device);
        m_materials[name] = material;
        return material;
    }

    std::shared_ptr<LveMaterial> LveMaterialManager::getMaterial(const std::string &name)
    {
        auto it = m_materials.find(name);
        if (it != m_materials.end())
        {
            return it->second;
        }
        return nullptr;
    }

    void LveMaterialManager::removeMaterial(const std::string &name)
    {
        m_materials.erase(name);
    }

    void LveMaterialManager::buildAllMaterials()
    {
        for (auto &[name, material] : m_materials)
        {
            material->buildDescriptorSet();
        }
    }

} // namespace lve