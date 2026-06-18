// Vulkan学习项目 — 雨滴粒子系统（精简版）
// 仅含雨滴特有逻辑，共享资源由 LveParticleSystem 打包器管理

#pragma once

#include "lve_device.hpp"
#include "lve_pipeline.hpp"
#include "lve_buffer.hpp"
#include "lve_descriptors.hpp"
#include "lve_frame_info.hpp"
#include "lve_model.hpp"
#include "lve_texture.hpp"
#include "lve_weather_state.hpp"
#include <memory>
#include <glm/glm.hpp>

namespace lve
{
    struct RainParticle {
        glm::vec4 position;
        glm::vec4 velocity;
        glm::vec4 color;
        glm::vec4 startColor;
        glm::vec4 endColor;
    };

    struct RainUbo {
        alignas(16) glm::vec3 emitterPositionMin;
        float deltaTime;
        alignas(16) glm::vec3 emitterPositionMax;
        float gravity;
        alignas(16) glm::vec3 windDirection;
        float windStrength;
        alignas(16) glm::vec3 cameraPosition;
        uint32_t particleCount;
        float time;
        float dropletSize;
        float dropletSpeed;
    };

    class LveRainSystem
    {
    public:
        LveRainSystem(LveDevice &device,
                      VkRenderPass renderPass,
                      VkDescriptorSetLayout globalSetLayout,
                      uint32_t maxParticles,
                      LveModel *quadModel);
        ~LveRainSystem();

        LveRainSystem(const LveRainSystem &) = delete;
        LveRainSystem &operator=(const LveRainSystem &) = delete;

        void updateAndRender(FrameInfo &frameInfo,
                             const glm::vec3 &emitterMin,
                             const glm::vec3 &emitterMax,
                             const WeatherPreset &weather,
                             uint32_t activeCount);

        uint32_t getMaxParticles() const { return m_maxParticles; }

    private:
        void createComputePipeline();
        void createGraphicsPipeline(VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        void createBuffers();
        void createDescriptorSets();
        void loadTexture();

        LveDevice &lveDevice;
        uint32_t m_maxParticles;
        LveModel *m_quadModel; // 共享四边形网格

        // particle SSBO + UBO
        std::unique_ptr<LveBuffer> m_particleBuffer;
        std::unique_ptr<LveBuffer> m_uboBuffer;

        // compute pipeline
        std::unique_ptr<LveDescriptorSetLayout> m_computeSetLayout;
        VkDescriptorSet m_computeSet = VK_NULL_HANDLE;
        VkPipelineLayout m_computePipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_computePipeline = VK_NULL_HANDLE;
        std::unique_ptr<LveDescriptorPool> m_computePool;

        // graphics pipeline + texture descriptor (set=1)
        std::unique_ptr<LvePipeline> m_graphicsPipeline;
        VkPipelineLayout m_graphicsPipelineLayout = VK_NULL_HANDLE;

        std::unique_ptr<LveTexture> m_texture;
        std::unique_ptr<LveDescriptorSetLayout> m_textureSetLayout;
        VkDescriptorSet m_textureSet = VK_NULL_HANDLE;
        std::unique_ptr<LveDescriptorPool> m_texturePool;
    };

} // namespace lve