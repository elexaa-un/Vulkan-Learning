#pragma once

#include "lve_camera.hpp"
#include "lve_gameobject.hpp"
#include <vulkan/vulkan.h>

namespace lve
{
#define MAX_LIGHTS 10

    struct PointLight
    {
        alignas(16) glm::vec4 position; // 16×Ö½Ú
        alignas(16) glm::vec4 color;    // 16×Ö½Ú
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
        glm::vec4 ambientColor{1.f, 1.f, 1.f, .02f};
        int numLights;
        PointLight pointLights[MAX_LIGHTS];
    };

}