// Vulkan学习项目 — 天气状态管理器实现
// Phase 1 基础设施：预设初始化、过渡引擎、全局时间系统

#include "lve_weather_state.hpp"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cassert>

namespace lve
{
    WeatherStateManager::WeatherStateManager()
    {
        initializePresets();
    }

    void WeatherStateManager::initializePresets()
    {
        m_presets.clear();

        // ===============================
        // 晴天 (Clear)
        // ===============================
        {
            WeatherPreset p;
            p.name = "晴天";
            p.skyTopColor = glm::vec4{0.25f, 0.5f, 1.0f, 1.0f};
            p.skyHorizonColor = glm::vec4{0.6f, 0.75f, 1.0f, 1.0f};
            p.sunColor = glm::vec4{1.0f, 0.95f, 0.85f, 1.0f};
            p.sunIntensity = 1.0f;
            p.sunAngularDiameter = 1.0f;
            p.cloudCoverage = 0.05f;
            p.cloudBaseAltitude = 800.0f;
            p.cloudThickness = 200.0f;
            p.cloudDensity = 0.1f;
            p.cloudWindVelocity = glm::vec2{1.0f, 0.0f};
            p.fogDensity = 0.02f;
            p.atmosphericScattering = 1.0f;
            p.mieScattering = 0.3f;
            p.visibility = 5000.0f;
            p.precipitationRate = 0.0f;
            p.dropletSize = 1.0f;
            p.dropletSpeed = 5.0f;
            p.isSnow = false;
            p.ambientColor = glm::vec4{0.3f, 0.35f, 0.5f, 0.3f};
            p.ambientIntensity = 0.3f;
            p.groundWetness = 0.0f;
            p.snowAccumulation = 0.0f;
            p.lightningFrequency = 0.0f;
            p.lightningIntensity = 0.0f;
            p.thunderDelay = 3.0f;
            m_presets[WeatherType::Clear] = p;
        }

        // ===============================
        // 阴天 (Cloudy)
        // ===============================
        {
            WeatherPreset p;
            p.name = "阴天";
            p.skyTopColor = glm::vec4{0.5f, 0.5f, 0.55f, 1.0f};
            p.skyHorizonColor = glm::vec4{0.6f, 0.6f, 0.65f, 1.0f};
            p.sunColor = glm::vec4{0.8f, 0.8f, 0.75f, 1.0f};
            p.sunIntensity = 0.4f;
            p.sunAngularDiameter = 1.5f;
            p.cloudCoverage = 0.75f;
            p.cloudBaseAltitude = 600.0f;
            p.cloudThickness = 400.0f;
            p.cloudDensity = 0.6f;
            p.cloudWindVelocity = glm::vec2{0.8f, 0.3f};
            p.fogDensity = 0.15f;
            p.atmosphericScattering = 0.8f;
            p.mieScattering = 0.7f;
            p.visibility = 2000.0f;
            p.precipitationRate = 0.0f;
            p.dropletSize = 1.0f;
            p.dropletSpeed = 5.0f;
            p.isSnow = false;
            p.ambientColor = glm::vec4{0.4f, 0.42f, 0.45f, 0.5f};
            p.ambientIntensity = 0.5f;
            p.groundWetness = 0.1f;
            p.snowAccumulation = 0.0f;
            p.lightningFrequency = 0.0f;
            p.lightningIntensity = 0.0f;
            p.thunderDelay = 3.0f;
            m_presets[WeatherType::Cloudy] = p;
        }

        // ===============================
        // 雨天 (Rain)
        // ===============================
        {
            WeatherPreset p;
            p.name = "雨天";
            p.skyTopColor = glm::vec4{0.25f, 0.3f, 0.4f, 1.0f};
            p.skyHorizonColor = glm::vec4{0.35f, 0.4f, 0.5f, 1.0f};
            p.sunColor = glm::vec4{0.5f, 0.5f, 0.6f, 1.0f};
            p.sunIntensity = 0.2f;
            p.sunAngularDiameter = 2.0f;
            p.cloudCoverage = 0.90f;
            p.cloudBaseAltitude = 400.0f;
            p.cloudThickness = 600.0f;
            p.cloudDensity = 0.8f;
            p.cloudWindVelocity = glm::vec2{1.5f, 0.5f};
            p.fogDensity = 0.35f;
            p.atmosphericScattering = 0.5f;
            p.mieScattering = 0.9f;
            p.visibility = 500.0f;
            p.precipitationRate = 0.6f;
            p.dropletSize = 1.0f;
            p.dropletSpeed = 8.0f;
            p.isSnow = false;
            p.ambientColor = glm::vec4{0.2f, 0.22f, 0.3f, 0.25f};
            p.ambientIntensity = 0.25f;
            p.groundWetness = 0.7f;
            p.snowAccumulation = 0.0f;
            p.lightningFrequency = 0.0f;
            p.lightningIntensity = 0.0f;
            p.thunderDelay = 3.0f;
            m_presets[WeatherType::Rain] = p;
        }

        // ===============================
        // 暴风雨 (Storm)
        // ===============================
        {
            WeatherPreset p;
            p.name = "暴风雨";
            p.skyTopColor = glm::vec4{0.15f, 0.15f, 0.25f, 1.0f};
            p.skyHorizonColor = glm::vec4{0.2f, 0.2f, 0.35f, 1.0f};
            p.sunColor = glm::vec4{0.3f, 0.3f, 0.4f, 1.0f};
            p.sunIntensity = 0.05f;
            p.sunAngularDiameter = 2.5f;
            p.cloudCoverage = 0.95f;
            p.cloudBaseAltitude = 300.0f;
            p.cloudThickness = 800.0f;
            p.cloudDensity = 0.95f;
            p.cloudWindVelocity = glm::vec2{3.0f, 1.0f};
            p.fogDensity = 0.50f;
            p.atmosphericScattering = 0.3f;
            p.mieScattering = 0.95f;
            p.visibility = 200.0f;
            p.precipitationRate = 0.9f;
            p.dropletSize = 1.2f;
            p.dropletSpeed = 12.0f;
            p.isSnow = false;
            p.ambientColor = glm::vec4{0.15f, 0.12f, 0.2f, 0.15f};
            p.ambientIntensity = 0.15f;
            p.groundWetness = 0.9f;
            p.snowAccumulation = 0.0f;
            p.lightningFrequency = 0.15f;
            p.lightningIntensity = 2.0f;
            p.thunderDelay = 2.0f;
            m_presets[WeatherType::Storm] = p;
        }

        // ===============================
        // 雪天 (Snow)
        // ===============================
        {
            WeatherPreset p;
            p.name = "雪天";
            p.skyTopColor = glm::vec4{0.6f, 0.65f, 0.75f, 1.0f};
            p.skyHorizonColor = glm::vec4{0.7f, 0.75f, 0.85f, 1.0f};
            p.sunColor = glm::vec4{0.7f, 0.7f, 0.8f, 1.0f};
            p.sunIntensity = 0.3f;
            p.sunAngularDiameter = 1.5f;
            p.cloudCoverage = 0.85f;
            p.cloudBaseAltitude = 500.0f;
            p.cloudThickness = 500.0f;
            p.cloudDensity = 0.7f;
            p.cloudWindVelocity = glm::vec2{0.6f, 0.4f};
            p.fogDensity = 0.30f;
            p.atmosphericScattering = 0.7f;
            p.mieScattering = 0.8f;
            p.visibility = 800.0f;
            p.precipitationRate = 0.5f;
            p.dropletSize = 3.0f;      // 雪花比雨滴大
            p.dropletSpeed = 1.5f;     // 雪花下落慢
            p.isSnow = true;
            p.ambientColor = glm::vec4{0.35f, 0.38f, 0.45f, 0.5f};
            p.ambientIntensity = 0.5f;
            p.groundWetness = 0.0f;
            p.snowAccumulation = 0.6f;
            p.lightningFrequency = 0.0f;
            p.lightningIntensity = 0.0f;
            p.thunderDelay = 3.0f;
            m_presets[WeatherType::Snow] = p;
        }
    }

    const WeatherPreset &WeatherStateManager::getPreset(WeatherType type) const
    {
        auto it = m_presets.find(type);
        assert(it != m_presets.end() && "Weather preset not found!");
        return it->second;
    }

    void WeatherStateManager::transitionTo(WeatherType target, float duration)
    {
        if (target == m_currentType && !m_transitioning)
        {
            return; // 已经是目标天气，无需过渡
        }

        // 如果正在过渡中，从当前混合结果作为起点
        if (m_transitioning)
        {
            // 用当前混合结果覆盖 current preset
            WeatherPreset blended = getBlendedPreset();
            m_presets[m_currentType] = blended;
        }

        m_targetType = target;
        m_transitionDuration = glm::max(duration, 0.1f);
        m_transitionProgress = 0.0f;
        m_transitioning = true;
    }

    void WeatherStateManager::cancelTransition()
    {
        if (!m_transitioning) return;
        // 回退到当前类型
        m_targetType = m_currentType;
        m_transitionProgress = 1.0f;
        m_transitioning = false;
    }

    void WeatherStateManager::update(float deltaTime)
    {
        // ---- 全局时间系统 ----
        float scaledDelta = deltaTime * m_simulationTimeScale;
        m_simulationTime += scaledDelta;

        // 昼夜循环
        m_timeOfDay += scaledDelta * m_dayNightCycleSpeed;
        while (m_timeOfDay >= 24.0f) m_timeOfDay -= 24.0f;
        while (m_timeOfDay < 0.0f) m_timeOfDay += 24.0f;

        // ---- 过渡引擎 ----
        if (m_transitioning)
        {
            m_transitionProgress += deltaTime / m_transitionDuration;
            if (m_transitionProgress >= 1.0f)
            {
                m_transitionProgress = 1.0f;
                m_currentType = m_targetType;
                m_transitioning = false;
            }
        }

        // ---- 闪电系统 ----
        if (m_lightningActive > 0.0f)
        {
            m_lightningActive -= deltaTime;
            if (m_lightningActive < 0.0f) m_lightningActive = 0.0f;
        }

        const WeatherPreset &currentPreset = getPreset(m_currentType);
        if (currentPreset.lightningFrequency > 0.0f)
        {
            m_lightningTimer -= deltaTime;
            if (m_lightningTimer <= 0.0f)
            {
                // 触发闪电
                m_lightningActive = 0.15f; // 闪电持续 ~150ms
                // 随机下次闪电时间
                float avgInterval = 1.0f / currentPreset.lightningFrequency;
                float randomOffset = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * avgInterval;
                m_lightningTimer = avgInterval + randomOffset;
            }
        }
    }

    WeatherPreset WeatherStateManager::getBlendedPreset() const
    {
        if (!m_transitioning)
        {
            return getPreset(m_currentType);
        }

        const WeatherPreset &current = getPreset(m_currentType);
        const WeatherPreset &target = getPreset(m_targetType);
        float t = easeInOutCubic(m_transitionProgress);

        return lerpPresets(current, target, t);
    }

    WeatherPreset WeatherStateManager::lerpPresets(const WeatherPreset &a, const WeatherPreset &b, float t)
    {
        WeatherPreset result;

        // 名称保持当前天气为主
        result.name = (t < 0.5f) ? a.name : b.name;

        result.skyTopColor = glm::mix(a.skyTopColor, b.skyTopColor, t);
        result.skyHorizonColor = glm::mix(a.skyHorizonColor, b.skyHorizonColor, t);
        result.sunColor = glm::mix(a.sunColor, b.sunColor, t);
        result.sunIntensity = glm::mix(a.sunIntensity, b.sunIntensity, t);
        result.sunAngularDiameter = glm::mix(a.sunAngularDiameter, b.sunAngularDiameter, t);
        result.cloudCoverage = glm::mix(a.cloudCoverage, b.cloudCoverage, t);
        result.cloudBaseAltitude = glm::mix(a.cloudBaseAltitude, b.cloudBaseAltitude, t);
        result.cloudThickness = glm::mix(a.cloudThickness, b.cloudThickness, t);
        result.cloudDensity = glm::mix(a.cloudDensity, b.cloudDensity, t);
        result.cloudWindVelocity = glm::mix(a.cloudWindVelocity, b.cloudWindVelocity, t);
        result.fogDensity = glm::mix(a.fogDensity, b.fogDensity, t);
        result.atmosphericScattering = glm::mix(a.atmosphericScattering, b.atmosphericScattering, t);
        result.mieScattering = glm::mix(a.mieScattering, b.mieScattering, t);
        result.visibility = glm::mix(a.visibility, b.visibility, t);
        result.precipitationRate = glm::mix(a.precipitationRate, b.precipitationRate, t);
        result.dropletSize = glm::mix(a.dropletSize, b.dropletSize, t);
        result.dropletSpeed = glm::mix(a.dropletSpeed, b.dropletSpeed, t);

        // isSnow 特殊处理：在过渡中点根据前后天气决定
        if (t < 0.5f)
            result.isSnow = a.isSnow;
        else
            result.isSnow = b.isSnow;

        result.ambientColor = glm::mix(a.ambientColor, b.ambientColor, t);
        result.ambientIntensity = glm::mix(a.ambientIntensity, b.ambientIntensity, t);
        result.groundWetness = glm::mix(a.groundWetness, b.groundWetness, t);
        result.snowAccumulation = glm::mix(a.snowAccumulation, b.snowAccumulation, t);

        // 闪电仅在 Storm 占比 > 0.5 时激活
        result.lightningFrequency = (t > 0.5f) ? b.lightningFrequency : a.lightningFrequency;
        result.lightningIntensity = glm::mix(a.lightningIntensity, b.lightningIntensity, t);
        result.thunderDelay = glm::mix(a.thunderDelay, b.thunderDelay, t);

        return result;
    }

    float WeatherStateManager::easeInOutCubic(float t)
    {
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - glm::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }

    void WeatherStateManager::writeToGlobalUbo(GlobalUbo &ubo) const
    {
        WeatherPreset blended = getBlendedPreset();

        // 根据时间计算太阳方向
        // timeOfDay=12.0 为正午（太阳在正上方），timeOfDay=0/24 为午夜（太阳在正下方）
        float sunAngle = (m_timeOfDay / 24.0f) * 2.0f * glm::pi<float>() - glm::half_pi<float>();
        glm::vec3 sunDir{
            glm::cos(sunAngle) * 0.2f,
            glm::sin(sunAngle),
            0.5f
        };
        sunDir = glm::normalize(sunDir);

        ubo.weather.fogColor = glm::vec4(blended.skyHorizonColor.r,
                                         blended.skyHorizonColor.g,
                                         blended.skyHorizonColor.b,
                                         blended.fogDensity);
        ubo.weather.sunDirection = glm::vec4(sunDir, blended.sunIntensity);
        ubo.weather.sunColor = glm::vec4(blended.sunColor.r,
                                         blended.sunColor.g,
                                         blended.sunColor.b,
                                         0.0f);
        ubo.weather.weatherParams = glm::vec4(blended.cloudCoverage,
                                              blended.precipitationRate,
                                              m_timeOfDay,
                                              blended.isSnow ? 1.0f : 0.0f);

        // 环境光由天气驱动
        ubo.ambientColor = glm::vec4(blended.ambientColor.r,
                                     blended.ambientColor.g,
                                     blended.ambientColor.b,
                                     blended.ambientIntensity);
    }
} // namespace lve