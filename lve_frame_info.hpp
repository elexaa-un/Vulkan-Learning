#pragma once

#include "lve_camera.hpp"
#include "lve_gameobject.hpp"
#include <vulkan/vulkan.h>

namespace lve
{
// 最大点光源数量限制
#define MAX_LIGHTS 10

    // 点光源数据结构：包含位置和颜色，使用alignas(16)确保对齐以满足GPU Uniform缓冲区要求
    struct PointLight
    {
        alignas(16) glm::vec4 position; // 光源位置（世界空间），16字节对齐
        alignas(16) glm::vec4 color;    // 光源颜色+强度，16字节对齐
    };

    // 帧信息结构体：每帧渲染时传递的上下文数据
    struct FrameInfo
    {
        int frameInfo;                              // 当前帧索引
        float frameTime;                            // 当前帧运行时间（秒）
        VkCommandBuffer commandBuffer;              // 当前帧使用的Vulkan命令缓冲区
        LveCamera &camera;                          // 当前激活的摄像机引用
        VkDescriptorSet globalDescriptorSet;        // 全局描述符集（包含UBO等）
        LveGameObject::Map &gameObjects;            // 场景中所有游戏对象的映射表

        // 视锥体剔除统计（每帧由渲染系统更新）
        uint32_t culledCount = 0;   // 被剔除的对象数量
        uint32_t totalCount = 0;    // 场景中总对象数量
    };

    // 天气参数子结构体（vec4对齐，用于GlobalUbo中的天气数据打包）
    // 使用alignas(16)确保在Uniform Buffer中的16字节对齐
    struct WeatherData
    {
        alignas(16) glm::vec4 fogColor{0.8f, 0.85f, 0.9f, 0.0f};       // 雾颜色（RGB）+ 雾密度（A）
        alignas(16) glm::vec4 sunDirection{0.0f, 1.0f, 0.0f, 1.0f};    // 太阳方向（XYZ）+ 太阳强度（W）
        alignas(16) glm::vec4 sunColor{1.0f, 0.95f, 0.85f, 0.0f};      // 太阳光颜色（RGB）+ 填充
        alignas(16) glm::vec4 weatherParams{0.05f, 0.0f, 12.0f, 0.0f}; // 云覆盖率(X) 降水强度(Y) 时间(W=0~24)
    };

    // 全局Uniform缓冲区对象：包含每帧传递给所有着色器的全局数据
    struct GlobalUbo
    {
        glm::mat4 projection{1.f};       // 投影矩阵
        glm::mat4 view{1.f};             // 视图矩阵
        glm::mat4 inverseView{1.f};      // 逆视图矩阵（用于从裁剪空间反算世界空间）
        glm::mat4 lightViewProj{1.f};    // 光源的 View*Projection 矩阵，用于阴影贴图计算
        glm::vec4 ambientColor{1.f, 1.f, 1.f, .02f}; // 环境光颜色（RGB）+ 环境光强度（A）
        int numLights;                   // 当前激活的点光源数量
        PointLight pointLights[MAX_LIGHTS]; // 点光源数组
        WeatherData weather;             // 天气系统数据（Phase 1 新增）
    };

}
