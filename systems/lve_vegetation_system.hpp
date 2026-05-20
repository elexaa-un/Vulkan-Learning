// lve_vegetation_system.hpp
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
    struct WindPushConstantData
    {
        float windTime;       // 累计时间（秒）
        float windStrength;   // 风力强度 [0.0, 2.0]
        float windSpeed;      // 风速 [0.1, 5.0]
        float windDirectionX; // 风向 X 分量（归一化）
        float windDirectionZ; // 风向 Z 分量（归一化）
    };

    class LveVegetationSystem
    {
    public:
        LveVegetationSystem(LveDevice &device, VkRenderPass renderPass,
                            VkDescriptorSetLayout globalSetLayout,
                            const std::string &texturePath);
        ~LveVegetationSystem();

        void createInstances(const std::vector<VegetationInstance> &instances);
        void updateInstances(const std::vector<VegetationInstance> &instances);
        void render(FrameInfo &frameInfo, WindPushConstantData &windData);

    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout textureSetLayout);
        void createPipeline(VkRenderPass renderPass);
        void createInstanceBuffer();
        void createQuadModel();

        LveDevice &lveDevice;
        std::unique_ptr<LveModel> m_quadModel;
        std::unique_ptr<LvePipeline> m_pipeline;
        VkPipelineLayout m_pipelineLayout;

        // 实例化数据
        std::unique_ptr<LveBuffer> m_instanceBuffer;
        std::unique_ptr<LveDescriptorSetLayout> m_textureSetLayout;
        std::unique_ptr<LveDescriptorPool> m_texturePool;
        VkDescriptorSet m_textureSet = VK_NULL_HANDLE;
        std::shared_ptr<LveTexture> m_texture;
        uint32_t m_instanceCount{0};
    };

} // namespace lve