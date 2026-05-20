#include "lve_simple_renderer_system.hpp"
#include "lve_utils.hpp"
#include "lve_frustum_culling.hpp"
#include <stdexcept>
#include <array>
#include <iostream>

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

        VK_CHECK(vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout));
    }

    void LveSimpleRenderSystem::createPipeline(VkRenderPass renderPass)
    {
        LVE_ASSERT(pipelineLayout != nullptr, "Cannot create pipeline before pipeline layout");

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
        // ===== 步骤 1：从相机 View-Projection 矩阵构造视锥体 =====
        // MVP = Projection * View，视锥体以此矩阵定义裁剪空间范围
        Frustum frustum(frameInfo.camera.getProjection() * frameInfo.camera.getView());

        // ===== 步骤 2：初始化剔除统计 =====
        frameInfo.totalCount = 0;
        frameInfo.culledCount = 0;

        lvePipeline->bind(frameInfo.commandBuffer);

        // 绑定全局描述符集 (set=0) —— 只需绑定一次，所有 gameObject 共享
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
                continue; // 没有模型的物体直接跳过
            }

            // ===== 步骤 3：视锥体剔除检测 =====
            ++frameInfo.totalCount; // 统计总数

            // 核心：将模型的局部空间 AABB 通过世界变换矩阵转换到世界空间
            //                局部AABB ——modelMatrix——> 世界空间AABB
            AABB worldAABB = gameObject.model->getAABB().transform(gameObject.transform.mat4());

            if (!frustum.isAABBVisible(worldAABB))
            {
                // 物体完全在视锥体之外，跳过渲染（这就是"剔除"）
                ++frameInfo.culledCount;
                continue; // 节省一次 Draw Call
            }
            // ===== 剔除检测结束 =====

            if (gameObject.getMaterial() && gameObject.getMaterial()->isBuilt())
            {
                VkDescriptorSet materialSet = gameObject.getMaterial()->getDescriptorSet();
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
