// Vulkan学习项目 — 粒子系统打包器
// 管理雨滴/雪花子系统的生命周期（延迟加载/释放），提供共享资源

#pragma once

#include "lve_device.hpp"
#include "lve_pipeline.hpp"
#include "lve_buffer.hpp"
#include "lve_descriptors.hpp"
#include "lve_frame_info.hpp"
#include "lve_model.hpp"
#include "lve_weather_state.hpp"
#include <memory>
#include <glm/glm.hpp>

namespace lve
{
    class LveRainSystem;
    class LveSnowSystem;

    // 粒子系统打包器：按需创建/释放雨滴和雪花子系统
    class LveParticleSystem
    {
    public:
        LveParticleSystem(LveDevice &device,
                          VkRenderPass renderPass,
                          VkDescriptorSetLayout globalSetLayout,
                          uint32_t maxRainParticles,
                          uint32_t maxSnowParticles);
        ~LveParticleSystem();

        LveParticleSystem(const LveParticleSystem &) = delete;
        LveParticleSystem &operator=(const LveParticleSystem &) = delete;

        void updateAndRender(FrameInfo &frameInfo,
                             const glm::vec3 &emitterMin,
                             const glm::vec3 &emitterMax,
                             const WeatherPreset &weather);

        uint32_t getMaxRainParticles() const { return m_maxRainParticles; }
        uint32_t getMaxSnowParticles() const { return m_maxSnowParticles; }

    private:
        void ensureRain();
        void ensureSnow();
        void releaseRain();
        void releaseSnow();

        // 创建基础四边形网格（共享）
        std::unique_ptr<LveModel> createQuadModel();

        LveDevice &m_device;
        VkRenderPass m_renderPass;
        VkDescriptorSetLayout m_globalSetLayout;
        uint32_t m_maxRainParticles;
        uint32_t m_maxSnowParticles;

        std::unique_ptr<LveRainSystem> m_rain;
        std::unique_ptr<LveSnowSystem> m_snow;

        // 共享四边形网格
        std::unique_ptr<LveModel> m_quadModel;
    };

} // namespace lve