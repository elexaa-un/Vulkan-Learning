#pragma once

#include "lve_camera.hpp"
#include "lve_gameobject.hpp"
#include <vulkan/vulkan.h>

namespace lve
{
#define MAX_LIGHTS 10

    struct PointLight
    {
        alignas(16) glm::vec4 position; // 16字节
        alignas(16) glm::vec4 color;    // 16字节
    };
    struct FrameInfo
    {
        int frameInfo;
        float frameTime;
        VkCommandBuffer commandBuffer;
        LveCamera &camera;
        VkDescriptorSet globalDescriptorSet;
        LveGameObject::Map &gameObjects;
    };
    struct GlobalUbo
    {
        glm::mat4 projection{1.f};
        glm::mat4 view{1.f};
        glm::mat4 inverseView{1.f};
        glm::mat4 lightViewProj{1.f};  // ===== Shadow Map: 光源的 View*Projection 矩阵 =====
        glm::vec4 ambientColor{1.f, 1.f, 1.f, .02f};
        int numLights;
        PointLight pointLights[MAX_LIGHTS];

        // ===== 新增：风力参数 =====
        float windTime;       // 累计时间（秒）
        float windStrength;   // 风力强度 [0.0, 2.0]
        float windSpeed;      // 风速 [0.1, 5.0]
        float windDirectionX; // 风向 X 分量（归一化）
        float windDirectionZ; // 风向 Z 分量（归一化）
        float _pad[3];        // 对齐填充（保证16字节对齐）
    };

}
