// Vulkan学习项目 — 天空盒渲染系统
// 使用立方体贴图渲染天空盒，始终以相机位置为中心

#pragma once
#include "lve_pipeline.hpp"
#include "lve_device.hpp"
#include "lve_gameobject.hpp"
#include "lve_camera.hpp"
#include "lve_texture.hpp"
#include "lve_frame_info.hpp"

namespace lve
{
    // 天空盒系统：使用立方体贴图纹理和专用着色器渲染天空盒
    class LveSkyboxSystem
    {
    public:
        LveSkyboxSystem(LveDevice &device, VkRenderPass renderPass,
                        VkDescriptorSetLayout globalSetLayout);
        ~LveSkyboxSystem();

        // 渲染天空盒：将天空盒移到相机位置，确保始终包裹场景
        void render(FrameInfo &frameInfo, std::shared_ptr<LveTexture> cubemap, LveModel &skyboxModel);

    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline(VkRenderPass renderPass);

        LveDevice &lveDevice;
        std::unique_ptr<LvePipeline> lvePipeline; // 天空盒渲染管线
        VkPipelineLayout pipelineLayout;           // 管线布局
    };
}
