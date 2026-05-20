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

        // 视锥体剔除统计（每帧由渲染系统更新）
        uint32_t culledCount = 0;
        uint32_t totalCount = 0;
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
    };

}
