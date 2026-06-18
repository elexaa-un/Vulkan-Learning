// Vulkan学习项目 — 粒子系统打包器实现
// 按需延迟加载/释放子系统，管理共享资源

#include "lve_particle_system.hpp"
#include "lve_rain_system.hpp"
#include "lve_snow_system.hpp"
#include <iostream>

namespace lve
{
    LveParticleSystem::LveParticleSystem(LveDevice &device,
                                         VkRenderPass renderPass,
                                         VkDescriptorSetLayout globalSetLayout,
                                         uint32_t maxRainParticles,
                                         uint32_t maxSnowParticles)
        : m_device(device), m_renderPass(renderPass),
          m_globalSetLayout(globalSetLayout),
          m_maxRainParticles(maxRainParticles),
          m_maxSnowParticles(maxSnowParticles)
    {
        std::cout << "[ParticleSystem] Packager created (rain=" << maxRainParticles
                  << ", snow=" << maxSnowParticles << "), subsystems lazy-loaded." << std::endl;

        // 共享四边形网格只创建一次
        m_quadModel = createQuadModel();
    }

    LveParticleSystem::~LveParticleSystem()
    {
        releaseRain();
        releaseSnow();
    }

    std::unique_ptr<LveModel> LveParticleSystem::createQuadModel()
    {
        std::vector<LveModel::Vertex> vertices(4);
        vertices[0].position = {-0.5f, -0.5f, 0.0f};
        vertices[0].color = {1.0f, 1.0f, 1.0f};
        vertices[0].normal = {0.0f, 0.0f, 1.0f};
        vertices[0].uv = {0.0f, 0.0f};
        vertices[1].position = {0.5f, -0.5f, 0.0f};
        vertices[1].color = {1.0f, 1.0f, 1.0f};
        vertices[1].normal = {0.0f, 0.0f, 1.0f};
        vertices[1].uv = {1.0f, 0.0f};
        vertices[2].position = {0.5f, 0.5f, 0.0f};
        vertices[2].color = {1.0f, 1.0f, 1.0f};
        vertices[2].normal = {0.0f, 0.0f, 1.0f};
        vertices[2].uv = {1.0f, 1.0f};
        vertices[3].position = {-0.5f, 0.5f, 0.0f};
        vertices[3].color = {1.0f, 1.0f, 1.0f};
        vertices[3].normal = {0.0f, 0.0f, 1.0f};
        vertices[3].uv = {0.0f, 1.0f};
        std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
        return LveModel::createModelFromVertices(m_device, vertices, indices);
    }

    void LveParticleSystem::ensureRain()
    {
        if (!m_rain)
        {
            std::cout << "[ParticleSystem] Lazy-loading rain system..." << std::endl;
            m_rain = std::make_unique<LveRainSystem>(
                m_device, m_renderPass, m_globalSetLayout,
                m_maxRainParticles, m_quadModel.get());
        }
    }

    void LveParticleSystem::ensureSnow()
    {
        if (!m_snow)
        {
            std::cout << "[ParticleSystem] Lazy-loading snow system..." << std::endl;
            m_snow = std::make_unique<LveSnowSystem>(
                m_device, m_renderPass, m_globalSetLayout,
                m_maxSnowParticles, m_quadModel.get());
        }
    }

    void LveParticleSystem::releaseRain()
    {
        m_rain.reset();
    }

    void LveParticleSystem::releaseSnow()
    {
        m_snow.reset();
    }

    void LveParticleSystem::updateAndRender(FrameInfo &frameInfo,
                                             const glm::vec3 &emitterMin,
                                             const glm::vec3 &emitterMax,
                                             const WeatherPreset &weather)
    {
        if (weather.isSnow)
        {
            // 雪天：延迟加载雪花系统，不释放雨滴（避免GPU资源校验报错）
            ensureSnow();
            uint32_t count = static_cast<uint32_t>(m_maxSnowParticles * weather.precipitationRate);
            m_snow->updateAndRender(frameInfo, emitterMin, emitterMax, weather, count);
        }
        else
        {
            // 非雪天：延迟加载雨滴系统，不释放雪花
            if (weather.precipitationRate > 0.001f)
            {
                ensureRain();
                uint32_t count = static_cast<uint32_t>(m_maxRainParticles * weather.precipitationRate);
                m_rain->updateAndRender(frameInfo, emitterMin, emitterMax, weather, count);
            }
        }
    }

} // namespace lve