#include "lve_skybox_system.hpp"
#include "lve_model.hpp"
#include <stdexcept>
#include <iostream>

namespace lve
{
    LveSkyboxSystem::LveSkyboxSystem(LveDevice &device, VkRenderPass renderPass,
                                     VkDescriptorSetLayout globalSetLayout)
        : lveDevice(device)
    {
        createPipelineLayout(globalSetLayout);
        createPipeline(renderPass);
    }

    LveSkyboxSystem::~LveSkyboxSystem()
    {
        if (pipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
        }
    }

    void LveSkyboxSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(glm::mat4); // 只传递视图矩阵（无平移）

        std::vector<VkDescriptorSetLayout> layouts = {globalSetLayout};
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
        layoutInfo.pSetLayouts = layouts.data();
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(lveDevice.device(), &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create skybox pipeline layout");
        }
    }

    void LveSkyboxSystem::createPipeline(VkRenderPass renderPass)
    {
        PipelineConfigInfo config{};
        LvePipeline::defaultPipelineConfigInfo(config);
        config.renderPass = renderPass;
        config.pipelineLayout = pipelineLayout;
        config.depthStencilInfo.depthWriteEnable = VK_FALSE; // 关键：不写深度
        config.depthStencilInfo.depthTestEnable = VK_TRUE;   // 但进行深度测试
        config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        lvePipeline = std::make_unique<LvePipeline>(lveDevice,
                                                    "shaders/skybox.vert.spv",
                                                    "shaders/skybox.frag.spv",
                                                    config);
    }

    void LveSkyboxSystem::render(FrameInfo &frameInfo, std::shared_ptr<LveTexture> cubemap, LveModel &skyboxModel)
    {
        if (!cubemap || !cubemap->isValid())
        {
            std::cerr << "Warning: Skybox cubemap not valid, skipping render" << std::endl;
            return;
        }

        lvePipeline->bind(frameInfo.commandBuffer);

        // 绑定全局描述符集（set=0），包含 UBO 和天空盒纹理
        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0, 1,
            &frameInfo.globalDescriptorSet,
            0, nullptr);

        // 计算去除平移的视图矩阵
        glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(frameInfo.camera.getView()));
        vkCmdPushConstants(frameInfo.commandBuffer,
                           pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof(glm::mat4),
                           &viewNoTranslation);

        // 绑定模型的顶点/索引缓冲区并绘制
        skyboxModel.bind(frameInfo.commandBuffer);
        skyboxModel.draw(frameInfo.commandBuffer);
    }

} // namespace lve