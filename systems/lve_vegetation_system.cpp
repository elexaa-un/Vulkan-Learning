// Vulkan学习项目 — 植被渲染系统实现
// GPU实例化渲染、风力动画和植被SSBO管理

#include "lve_vegetation_system.hpp"
#include "lve_model.hpp"
#include <stdexcept>
#include <iostream>

namespace lve
{

    LveVegetationSystem::LveVegetationSystem(LveDevice &device, VkRenderPass renderPass,
                                             VkDescriptorSetLayout globalSetLayout,
                                             const std::string &texturePath)
        : lveDevice(device)
    {
        // 1. 创建纹理描述符集布局
        m_textureSetLayout = LveDescriptorSetLayout::Builder(device)
                                 .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                 .build();

        // 2. 创建描述符池
        m_texturePool = LveDescriptorPool::Builder(device)
                            .setMaxSets(1)
                            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
                            .build();

        // 3. 加载纹理
        m_texture = std::make_shared<LveTexture>(device, texturePath);

        // 4. 创建描述符集
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_texture->getImageView();
        imageInfo.sampler = m_texture->getSampler();

        LveDescriptorWriter(*m_textureSetLayout, *m_texturePool)
            .writeImage(0, &imageInfo)
            .build(m_textureSet);

        // 5. 创建管线布局（需要两个 set：globalSetLayout 和 m_textureSetLayout）
        createPipelineLayout(globalSetLayout, m_textureSetLayout->getDescriptorSetLayout());
        createPipeline(renderPass);
        createQuadModel();
    }

    LveVegetationSystem::~LveVegetationSystem()
    {
        if (m_pipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(lveDevice.device(), m_pipelineLayout, nullptr);
        }
    }

    void LveVegetationSystem::createQuadModel()
    {
        std::vector<LveModel::Vertex> vertices(4);
        // 顶点0: 左下(地面/树根) — Y=0, UV.y=0=纹理底部
        vertices[0].position = {-0.5f, 0.0f, 0.0f};
        vertices[0].color = {1.0f, 1.0f, 1.0f};
        vertices[0].normal = {0.0f, 0.0f, 1.0f};
        vertices[0].uv = {0.0f, 0.0f};
        // 顶点1: 右下(地面/树根)
        vertices[1].position = {0.5f, 0.0f, 0.0f};
        vertices[1].color = {1.0f, 1.0f, 1.0f};
        vertices[1].normal = {0.0f, 0.0f, 1.0f};
        vertices[1].uv = {1.0f, 0.0f};
        // 顶点2: 右上(顶部/树冠) — Y=1, UV.y=1=纹理顶部
        vertices[2].position = {0.5f, 1.0f, 0.0f};
        vertices[2].color = {1.0f, 1.0f, 1.0f};
        vertices[2].normal = {0.0f, 0.0f, 1.0f};
        vertices[2].uv = {1.0f, 1.0f};
        // 顶点3: 左上(顶部/树冠)
        vertices[3].position = {-0.5f, 1.0f, 0.0f};
        vertices[3].color = {1.0f, 1.0f, 1.0f};
        vertices[3].normal = {0.0f, 0.0f, 1.0f};
        vertices[3].uv = {0.0f, 1.0f};

        std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
        m_quadModel = LveModel::createModelFromVertices(lveDevice, vertices, indices);
        std::cout << "Quad model created with " << vertices.size() << " vertices and " << indices.size() << " indices." << std::endl;
    }

    void LveVegetationSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout textureSetLayout)
    {
        // 使用两个描述符集：
        // set 0: global UBO (由 FirstApp 提供)
        // set 1: 植物纹理 (由调用者传入，每个植物类型一个)

        std::vector<VkDescriptorSetLayout> layouts = {globalSetLayout, textureSetLayout};

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(WindPushConstantData); // 传递风动画数据

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
        layoutInfo.pSetLayouts = layouts.data();
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(lveDevice.device(), &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create vegetation pipeline layout");
        }
    }

    void LveVegetationSystem::createPipeline(VkRenderPass renderPass)
    {
        PipelineConfigInfo config{};
        LvePipeline::defaultPipelineConfigInfo(config); // 先获得默认设置（如深度、光栅化等）

        // 修改几何体相关设置
        config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        config.depthStencilInfo.depthWriteEnable = VK_FALSE;
        config.depthStencilInfo.depthTestEnable = VK_TRUE;
        LvePipeline::enablePipelineAlphaBlending(config);

        // ---------- 关键：自定义顶点输入描述 ----------
        // 1. binding 0: 四边形模型的顶点数据（每个顶点一次）
        std::vector<VkVertexInputBindingDescription> bindings(2);
        bindings[0] = LveModel::Vertex::getBindingDescriptions()[0]; // binding 0, stride=sizeof(Vertex), inputRate=VERTEX

        // 2. binding 1: 实例数据（每个实例一次）
        bindings[1].binding = 1;
        bindings[1].stride = sizeof(VegetationInstance); // 16字节 (vec3 + float)
        bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        // 属性描述：前4个是普通顶点属性（location 0~3）
        std::vector<VkVertexInputAttributeDescription> attributes =
            LveModel::Vertex::getAttributeDescriptions(); // location 0,1,2,3

        // 添加实例属性：位置 (vec3) 和 缩放 (float)
        VkVertexInputAttributeDescription instancePosAttr{};
        instancePosAttr.location = 4;
        instancePosAttr.binding = 1;
        instancePosAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        instancePosAttr.offset = offsetof(VegetationInstance, position);

        VkVertexInputAttributeDescription instanceScaleAttr{};
        instanceScaleAttr.location = 5;
        instanceScaleAttr.binding = 1;
        instanceScaleAttr.format = VK_FORMAT_R32_SFLOAT;
        instanceScaleAttr.offset = offsetof(VegetationInstance, scale);

        attributes.push_back(instancePosAttr);
        attributes.push_back(instanceScaleAttr);
        // 注意：location 6 (windFlexibility) 已移除
        // vegetation.vert 仅使用 location 0-5，不需要此属性
        // 覆盖 config 中的绑定和属性
        config.bindingDescriptions = bindings;
        config.attributeDescriptions = attributes;
        config.vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
        config.vertexInputInfo.pVertexBindingDescriptions = bindings.data();
        config.vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        config.vertexInputInfo.pVertexAttributeDescriptions = attributes.data();

        // 创建管线
        config.pipelineLayout = m_pipelineLayout;
        config.renderPass = renderPass;
        m_pipeline = std::make_unique<LvePipeline>(lveDevice,
                                                   "shaders/vegetation.vert.spv",
                                                   "shaders/vegetation.frag.spv",
                                                   config);
    }
    void LveVegetationSystem::createInstanceBuffer()
    {
        if (!m_instanceCount)
            return;

        VkDeviceSize instanceSize = sizeof(VegetationInstance); // 每个实例的大小
        m_instanceBuffer = std::make_unique<LveBuffer>(
            lveDevice,
            instanceSize,    // 正确：每个实例的大小
            m_instanceCount, // 正确：实例数量
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

    void LveVegetationSystem::createInstances(const std::vector<VegetationInstance> &instances)
    {
        m_instanceCount = static_cast<uint32_t>(instances.size());
        createInstanceBuffer();

        // 写入数据（注意总大小为 instanceSize * m_instanceCount）
        m_instanceBuffer->map();
        m_instanceBuffer->writeToBuffer(const_cast<VegetationInstance *>(instances.data()),
                                        sizeof(VegetationInstance) * m_instanceCount);
        m_instanceBuffer->unmap();
    }

    void LveVegetationSystem::updateInstances(const std::vector<VegetationInstance> &instances)
    {
        if (instances.size() != m_instanceCount)
        {
            createInstances(instances); // 大小变化，重新创建
        }
        else
        {
            m_instanceBuffer->map();
            m_instanceBuffer->writeToBuffer(const_cast<VegetationInstance *>(instances.data()));
            m_instanceBuffer->unmap();
        }
    }

    void LveVegetationSystem::render(FrameInfo &frameInfo, WindPushConstantData &windData)
    {
        if (m_instanceCount == 0)
            return;

        m_pipeline->bind(frameInfo.commandBuffer);

        // 绑定全局描述符集 (set=0)
        vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_pipelineLayout,
                                0, 1,
                                &frameInfo.globalDescriptorSet,
                                0, nullptr);

        // 绑定纹理描述符集 (set=1)
        vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_pipelineLayout,
                                1, 1,
                                &m_textureSet,
                                0, nullptr);
        // 将风数据通过 push constant 传递给着色器
        vkCmdPushConstants(frameInfo.commandBuffer,
                           m_pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(WindPushConstantData),
                           &windData);

        // 绑定几何顶点缓冲（四边形）
        m_quadModel->bind(frameInfo.commandBuffer);

        // 绑定实例缓冲作为顶点缓冲（绑定位置1，用于实例化数据）
        VkBuffer instanceBuffers[] = {m_instanceBuffer->getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(frameInfo.commandBuffer, 1, 1, instanceBuffers, offsets);

        // 绘制实例
        vkCmdDrawIndexed(frameInfo.commandBuffer,
                         m_quadModel->getIndexCount(),
                         m_instanceCount,
                         0, 0, 0);
    }
} // namespace lve
