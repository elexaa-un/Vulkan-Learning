// Vulkan学习项目 — 植被渲染系统
// 使用GPU实例化技术渲染大量植被（草、树等），支持风力动画

#pragma once

#include "lve_pipeline.hpp"
#include "lve_device.hpp"
#include "lve_camera.hpp"
#include "lve_texture.hpp"
#include "lve_frame_info.hpp"
#include "vegetation_instance.hpp"
#include "lve_model.hpp"
#include <vector>
#include <memory>

namespace lve
{
    // 风力推送常量：通过PushConstant传递给植被着色器
    struct WindPushConstantData
    {
        float windTime;       // 累计时间（秒），用于正弦波动画
        float windStrength;   // 风力强度 [0.0, 2.0]
        float windSpeed;      // 风速 [0.1, 5.0]
        float windDirectionX; // 风向 X 分量（归一化）
        float windDirectionZ; // 风向 Z 分量（归一化）
    };

    // 植被系统：管理大量植被实例的GPU缓冲区和渲染管线
    class LveVegetationSystem
    {
    public:
        LveVegetationSystem(LveDevice &device, VkRenderPass renderPass,
                            VkDescriptorSetLayout globalSetLayout,
                            const std::string &texturePath);
        ~LveVegetationSystem();

        // 创建/更新植被实例缓冲区（SSBO）
        void createInstances(const std::vector<VegetationInstance> &instances);
        void updateInstances(const std::vector<VegetationInstance> &instances);

        // 渲染所有植被实例（使用vkCmdDrawIndexed间接绘制）
        void render(FrameInfo &frameInfo, WindPushConstantData &windData);

    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout textureSetLayout);
        void createPipeline(VkRenderPass renderPass);
        void createInstanceBuffer(); // 创建SSBO实例缓冲区
        void createQuadModel();       // 创建基础四边形网格

        LveDevice &lveDevice;
        std::unique_ptr<LveModel> m_quadModel;       // 基础四边形模型（每个实例复用的几何体）
        std::unique_ptr<LvePipeline> m_pipeline;     // 植被渲染管线
        VkPipelineLayout m_pipelineLayout;           // 管线布局

        // 实例化数据
        std::unique_ptr<LveBuffer> m_instanceBuffer;           // 实例数据SSBO
        std::unique_ptr<LveDescriptorSetLayout> m_textureSetLayout; // 纹理描述符布局
        std::unique_ptr<LveDescriptorPool> m_texturePool;      // 纹理描述符池
        VkDescriptorSet m_textureSet = VK_NULL_HANDLE;         // 纹理描述符集
        std::shared_ptr<LveTexture> m_texture;                 // 植被纹理
        uint32_t m_instanceCount{0};                           // 当前实例数量
    };

} // namespace lve
