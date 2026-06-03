// Vulkan学习项目 — 点光源渲染系统
// 将场景中的点光源对象渲染为发光球体，支持延迟光照效果

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
    // 点光源系统：使用专用着色器渲染点光源球体，并更新全局UBO中的光源数据
    class LvePointLightSystem
    {
    public:
        // 构造函数：创建点光源管线和管线布局
        LvePointLightSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        ~LvePointLightSystem();

        LvePointLightSystem(const LvePointLightSystem &) = delete;
        LvePointLightSystem &operator=(const LvePointLightSystem &) = delete;

        // 更新全局UBO中的点光源数据
        void update(FrameInfo &frameInfo, GlobalUbo &ubo);

        // 渲染所有点光源对象
        void renderer(FrameInfo &frameInfo);

    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline(VkRenderPass renderPass);

        LveDevice &lveDevice;
        std::unique_ptr<LvePipeline> lvePipeline; // 点光源渲染管线
        VkPipelineLayout pipelineLayout;           // 管线布局
    };
}
