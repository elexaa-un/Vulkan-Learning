// Vulkan学习项目 — 天气状态管理器
// 管理天气类型枚举、预设参数、状态过渡引擎和全局时间系统
// Phase 1 基础设施

#pragma once

#include "lve_frame_info.hpp"
#include <glm/glm.hpp>
#include <string>
#include <map>
#include <vector>

namespace lve
{
    enum class WeatherType : int
    {
        Clear = 0,  // 晴天
        Cloudy,      // 阴天
        Rain,        // 雨天
        Storm,       // 暴风雨
        Snow,        // 雪天
        Count        // （用于计数）
    };

    // 天气预设：定义一种天气类型的完整视觉参数
    struct WeatherPreset
    {
        std::string name;   // 显示名称

        // 天空参数
        glm::vec4 skyTopColor{0.3f, 0.5f, 1.0f, 1.0f};        // 天顶颜色
        glm::vec4 skyHorizonColor{0.7f, 0.8f, 1.0f, 1.0f};     // 地平线颜色
        glm::vec4 sunColor{1.0f, 0.95f, 0.85f, 1.0f};          // 太阳光颜色
        float sunIntensity = 1.0f;                               // 太阳强度 (0.0 ~ 1.0)
        float sunAngularDiameter = 1.0f;                         // 太阳视直径 (度)

        // 云层参数
        float cloudCoverage = 0.05f;    // 云覆盖率 (0.0 ~ 1.0)
        float cloudBaseAltitude = 500.0f; // 云底高度 (m)
        float cloudThickness = 300.0f;    // 云层厚度 (m)
        float cloudDensity = 0.1f;        // 云密度
        glm::vec2 cloudWindVelocity{1.0f, 0.0f}; // 云层风速 (m/s, xz平面)

        // 大气参数
        float fogDensity = 0.02f;          // 雾密度 (0.0 ~ 1.0)
        float atmosphericScattering = 1.0f; // 大气散射强度
        float mieScattering = 0.5f;         // Mie散射 (朦胧感)
        float visibility = 5000.0f;         // 能见度 (m)

        // 降水参数
        float precipitationRate = 0.0f;   // 降水强度 (0.0 ~ 1.0)
        float dropletSize = 1.0f;          // 水滴/雪花大小
        float dropletSpeed = 5.0f;         // 下落速度
        bool  isSnow = false;              // true=雪, false=雨

        // 环境光参数
        glm::vec4 ambientColor{0.3f, 0.35f, 0.5f, 0.3f};  // 环境光颜色 (受天空散射影响)
        float ambientIntensity = 0.3f;                       // 环境光强度

        // 地面响应参数
        float groundWetness = 0.0f;       // 地面湿润度 (0.0 ~ 1.0)
        float snowAccumulation = 0.0f;    // 积雪量 (0.0 ~ 1.0)

        // 特效参数
        float lightningFrequency = 0.0f;  // 闪电频率 (次/秒, 0=无闪电)
        float lightningIntensity = 0.0f;  // 闪电强度
        float thunderDelay = 3.0f;        // 雷声延迟 (秒)
    };

    // 天气状态管理器
    // 单例模式或成员类，管理预设表、过渡状态、全局时间系统
    class WeatherStateManager
    {
    public:
        WeatherStateManager();
        ~WeatherStateManager() = default;

        // 禁止拷贝
        WeatherStateManager(const WeatherStateManager &) = delete;
        WeatherStateManager &operator=(const WeatherStateManager &) = delete;

        // ---- 预设管理 ----
        void initializePresets();                                  // 初始化5种天气预设
        const WeatherPreset &getPreset(WeatherType type) const;    // 获取指定类型的预设
        const std::map<WeatherType, WeatherPreset> &getAllPresets() const { return m_presets; }

        // ---- 过渡引擎 ----
        void transitionTo(WeatherType target, float duration);    // 触发过渡
        void cancelTransition();                                   // 取消当前过渡
        void update(float deltaTime);                              // 每帧调用，驱动过渡进度
        WeatherPreset getBlendedPreset() const;                    // 获取当前混合后的预设

        // 过渡状态查询
        bool isTransitioning() const { return m_transitioning; }
        float getTransitionProgress() const { return m_transitionProgress; }
        float getTransitionDuration() const { return m_transitionDuration; }
        WeatherType getCurrentType() const { return m_currentType; }
        WeatherType getTargetType() const { return m_targetType; }

        // ---- 全局时间系统 ----
        void setTimeOfDay(float tod) { m_timeOfDay = glm::clamp(tod, 0.0f, 24.0f); }
        float getTimeOfDay() const { return m_timeOfDay; }
        void setDayNightCycleSpeed(float speed) { m_dayNightCycleSpeed = speed; }
        float getDayNightCycleSpeed() const { return m_dayNightCycleSpeed; }
        float getSimulationTime() const { return m_simulationTime; }
        void setSimulationTimeScale(float scale) { m_simulationTimeScale = scale; }
        float getSimulationTimeScale() const { return m_simulationTimeScale; }

        // ---- 快捷接口：将混合结果写入 GlobalUbo ----
        void writeToGlobalUbo(GlobalUbo &ubo) const;

    private:
        // 对两个 WeatherPreset 的所有字段执行 LERP 插值
        static WeatherPreset lerpPresets(const WeatherPreset &a, const WeatherPreset &b, float t);

        // 缓动函数 (Ease-In-Out)
        static float easeInOutCubic(float t);

        // 预设表
        std::map<WeatherType, WeatherPreset> m_presets;

        // 过渡状态
        bool m_transitioning = false;
        WeatherType m_currentType = WeatherType::Clear;
        WeatherType m_targetType = WeatherType::Clear;
        float m_transitionProgress = 1.0f;   // 0.0 ~ 1.0
        float m_transitionDuration = 3.0f;   // 过渡时长 (秒)

        // 全局时间系统
        float m_simulationTime = 0.0f;       // 模拟时间 (秒，支持加速/减速)
        float m_timeOfDay = 12.0f;           // 当前时间 (0.0~24.0 小时循环)
        float m_dayNightCycleSpeed = 0.02f;  // 昼夜循环速度 (小时/秒 real-time)
        float m_simulationTimeScale = 1.0f;  // 模拟时间缩放因子

        // 闪电系统
        float m_lightningTimer = 0.0f;       // 闪电计时器
        float m_lightningActive = 0.0f;      // 闪电激活剩余时间 (秒)
    };
} // namespace lve