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
    class LveSimpleRenderSystem
    {
    public:
        LveSimpleRenderSystem(LveDevice &device, VkRenderPass renderPass,
                              VkDescriptorSetLayout globalSetLayout,
                              VkDescriptorSetLayout materialSetLayout);
        ~LveSimpleRenderSystem();

        LveSimpleRenderSystem(const LveSimpleRenderSystem &) = delete;
        LveSimpleRenderSystem &operator=(const LveSimpleRenderSystem &) = delete;
        void rendererGameObjects(FrameInfo &frameInfo);
        VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }

    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout materialSetLayout);
        void createPipeline(VkRenderPass renderPass);

        LveDevice &lveDevice;
        std::unique_ptr<LvePipeline> lvePipeline;
        VkPipelineLayout pipelineLayout;
    };
}