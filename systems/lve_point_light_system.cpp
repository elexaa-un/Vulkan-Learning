#include "lve_point_light_system.hpp"
#include "lve_utils.hpp"
#include <stdexcept>
#include <array>
#include <iostream>
#include <map>
namespace lve
{
    struct PointLightPushComponents
    {
        glm::vec4 position{};
        glm::vec4 color{};
        float radius;
    };

    LvePointLightSystem::LvePointLightSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
        : lveDevice(device)
    {
        createPipelineLayout(globalSetLayout);
        createPipeline(renderPass);
    }

    LvePointLightSystem::~LvePointLightSystem()
    {
        vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
    }
    void LvePointLightSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PointLightPushComponents);

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

        VkPipelineLayoutCreateInfo pipelineLayoueInfo{};
        pipelineLayoueInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoueInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoueInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoueInfo.pushConstantRangeCount = 1;
        pipelineLayoueInfo.pPushConstantRanges = &pushConstantRange;

        VK_CHECK(vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoueInfo, nullptr, &pipelineLayout));
    }

    void LvePointLightSystem::createPipeline(VkRenderPass renderPass)
    {
        LVE_ASSERT(pipelineLayout != nullptr, "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo pipelineConfig{};
        LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
        LvePipeline::enablePipelineAlphaBlending(pipelineConfig);
        // 点光源使用 advertiser billboard，不需要顶点缓冲区
        pipelineConfig.attributeDescriptions.clear();
        pipelineConfig.bindingDescriptions.clear();
        // 点光源只写颜色，不写深度（使用 LESS_OR_EQUAL 保证叠加正确）
        pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        pipelineConfig.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        pipelineConfig.renderPass = renderPass;
        pipelineConfig.pipelineLayout = pipelineLayout;
        lvePipeline = std::make_unique<LvePipeline>(
            lveDevice,
            "shaders/point_light.vert.spv",
            "shaders/point_light.frag.spv",
            pipelineConfig);
    }

    void LvePointLightSystem::renderer(FrameInfo &frameInfo)
    {
        std::map<float, LveGameObject::id_t> sorted;
        for (auto &kv : frameInfo.gameObjects)
        {
            auto &obj = kv.second;
            if (obj.pointLight == nullptr)
            {
                continue;
            }

            auto offset = frameInfo.camera.getPosition() - obj.transform.translation;
            float disSquare = glm::dot(offset, offset);
            sorted[disSquare] = obj.getId();
        }

        lvePipeline->bind(frameInfo.commandBuffer);

        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0, 1,
            &frameInfo.globalDescriptorSet,
            0, nullptr);
        for (auto it = sorted.rbegin(); it != sorted.rend(); ++it)
        {
            auto &gameObj = frameInfo.gameObjects.at(it->second);

            PointLightPushComponents push{};
            push.position = glm::vec4(gameObj.transform.translation, 1.0f);
            push.color = glm::vec4(gameObj.color, gameObj.pointLight->lightIntensity);
            push.radius = gameObj.transform.scale.x;

            vkCmdPushConstants(frameInfo.commandBuffer,
                               pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(PointLightPushComponents),
                               &push);

            vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
        }
    }

    void LvePointLightSystem::update(FrameInfo &frameInfo, GlobalUbo &ubo)
    {
        int lightIndex = 0;
        for (auto &kv : frameInfo.gameObjects)
        {
            auto &obj = kv.second;
            if (obj.pointLight == nullptr)
                continue;

            LVE_ASSERT(lightIndex < MAX_LIGHTS, "Point lights exceed maximum specified");

            // update light position
            obj.transform.translation = glm::vec3(glm::vec4(obj.transform.translation, 1.f));

            // copy light to ubo
            ubo.pointLights[lightIndex].position = glm::vec4(obj.transform.translation, 1.f);
            ubo.pointLights[lightIndex].color = glm::vec4(obj.color, obj.pointLight->lightIntensity);

            lightIndex += 1;
        }
        ubo.numLights = lightIndex;
    }
}
