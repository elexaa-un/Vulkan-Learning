// Vulkan学习项目 — 基础渲染系统
// 使用PBR着色器渲染场景中的普通游戏对象（模型+纹理）

#pragma once

#include "../lve_pipeline.hpp"
#include "../lve_device.hpp"
#include "../lve_gameobject.hpp"
#include "../lve_camera.hpp"
#include "../lve_frame_info.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <memory>
#include <vector>
namespace lve
{
    // 基础渲染系统：使用simple_shader管线渲染带纹理的3D模型
    class LveSimpleRenderSystem
    {
    public:
        LveSimpleRenderSystem(LveDevice &device, VkRenderPass renderPass,
                              VkDescriptorSetLayout globalSetLayout,
                              VkDescriptorSetLayout materialSetLayout);
        ~LveSimpleRenderSystem();

        LveSimpleRenderSystem(const LveSimpleRenderSystem &) = delete;
        LveSimpleRenderSystem &operator=(const LveSimpleRenderSystem &) = delete;

        // 渲染所有普通游戏对象（遍历 gameObjects 并绑定模型/纹理/描述符集）
        void rendererGameObjects(FrameInfo &frameInfo);

        // 获取管线布局（用于 PushConstant 等）
        VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }

    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout materialSetLayout);
        void createPipeline(VkRenderPass renderPass);

        LveDevice &lveDevice;
        std::unique_ptr<LvePipeline> lvePipeline; // PBR着色管线
        VkPipelineLayout pipelineLayout;           // 管线布局（包含Global+Material描述符集）
    };
}
