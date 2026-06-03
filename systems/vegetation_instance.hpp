// Vulkan学习项目 — 植被实例数据结构
// 定义单个植被实例（草、树等）的GPU端数据布局

#pragma once
#include <glm/glm.hpp>

namespace lve
{
    // 植被实例结构体：存储在SSBO中，每个实例对应一株植被
    // 成员布局需与shader中的结构体定义保持一致
    struct VegetationInstance
    {
        glm::vec3 position{0.0f};    // 植被实例在世界空间中的位置
        float scale{1.0f};           // 植被实例的缩放因子
        float windFlexibility{1.0f}; // 风力柔韧度 [0.0, 1.0]，0=完全不受风力影响，1=风力影响最大
        float _pad[3];               // 对齐填充，确保结构体满足GPU的std430/std140对齐要求
    };
}
