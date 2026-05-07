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
    class LvePointLightSystem
    {
    public:
        LvePointLightSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        ~LvePointLightSystem();

        LvePointLightSystem(const LvePointLightSystem &) = delete;
        LvePointLightSystem &operator=(const LvePointLightSystem &) = delete;
        void update(FrameInfo &frameInfo, GlobalUbo &ubo);
        void renderer(FrameInfo &frameInfo);

    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline(VkRenderPass renderPass);

        LveDevice &lveDevice;
        std::unique_ptr<LvePipeline> lvePipeline;
        VkPipelineLayout pipelineLayout;
    };
}