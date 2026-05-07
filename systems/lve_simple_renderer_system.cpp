#include "lve_simple_renderer_system.hpp"
#include <stdexcept>
#include <array>
#include <iostream>
#include <cassert>

namespace lve
{
    // 扩展 Push Constant 结构体以包含材质参数
    struct SimplePushConstantData
    {
        glm::mat4 modelMatrix{1.f};
        glm::mat4 normalMatrix{1.f};
    };

    static_assert(sizeof(SimplePushConstantData) <= 128,
                  "Push constant size must be <= maxPushConstantSize (128 bytes)");

    LveSimpleRenderSystem::LveSimpleRenderSystem(LveDevice &device,
                                                 VkRenderPass renderPass,
                                                 VkDescriptorSetLayout globalSetLayout,
                                                 VkDescriptorSetLayout materialSetLayout)
        : lveDevice(device)
    {
        createPipelineLayout(globalSetLayout, materialSetLayout);
        createPipeline(renderPass);
    }

    LveSimpleRenderSystem::~LveSimpleRenderSystem()
    {
        vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
    }

    void LveSimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout,
                                                     VkDescriptorSetLayout materialSetLayout)
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(SimplePushConstantData);

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {globalSetLayout, materialSetLayout};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Pipeline Layout!");
        }
    }

    void LveSimpleRenderSystem::createPipeline(VkRenderPass renderPass)
    {
        assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo pipelineConfig{};
        LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.renderPass = renderPass;
        pipelineConfig.pipelineLayout = pipelineLayout;
        lvePipeline = std::make_unique<LvePipeline>(
            lveDevice,
            "shaders/simple_shader.vert.spv",
            "shaders/simple_shader.frag.spv",
            pipelineConfig);
    }

    void LveSimpleRenderSystem::rendererGameObjects(FrameInfo &frameInfo)
    {
        lvePipeline->bind(frameInfo.commandBuffer);

        // 绑定全局描述符集 (set=0)
        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0, 1,
            &frameInfo.globalDescriptorSet,
            0, nullptr);

        for (auto &kv : frameInfo.gameObjects)
        {
            auto &gameObject = kv.second;
            if (gameObject.model == nullptr)
            {
                continue;
            }
            if (gameObject.material && gameObject.material->isBuilt())
            {
                VkDescriptorSet materialSet = gameObject.material->getDescriptorSet();
                vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        pipelineLayout,
                                        1, // set = 1
                                        1, // descriptorSetCount = 1
                                        &materialSet,
                                        0, nullptr);
            }
            SimplePushConstantData push{};
            push.modelMatrix = gameObject.transform.mat4();
            push.normalMatrix = gameObject.transform.normalMartix();

            // 材质参数已通过描述符集传递，不再需要 push constant

            vkCmdPushConstants(frameInfo.commandBuffer,
                               pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(SimplePushConstantData),
                               &push);

            // 绑定模型并绘制
            gameObject.model->bind(frameInfo.commandBuffer);
            gameObject.model->draw(frameInfo.commandBuffer);
        }
    }
}