#pragma once
#include "lve_pipeline.hpp"
#include "lve_device.hpp"
#include "lve_gameobject.hpp"
#include "lve_camera.hpp"
#include "lve_texture.hpp"
#include "lve_frame_info.hpp"

namespace lve
{
    class LveSkyboxSystem
    {
    public:
        LveSkyboxSystem(LveDevice &device, VkRenderPass renderPass,
                        VkDescriptorSetLayout globalSetLayout);
        ~LveSkyboxSystem();

        void render(FrameInfo &frameInfo, std::shared_ptr<LveTexture> cubemap, LveModel &skyboxModel);

    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline(VkRenderPass renderPass);

        LveDevice &lveDevice;
        std::unique_ptr<LvePipeline> lvePipeline;
        VkPipelineLayout pipelineLayout;
    };
}