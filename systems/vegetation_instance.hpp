// vegetation_instance.hpp
#pragma once
#include <glm/glm.hpp>

namespace lve
{
    struct VegetationInstance
    {
        glm::vec3 position{0.0f};
        float scale{1.0f};
        float windFlexibility{1.0f}; // 新增：风力柔韧度 [0.0, 1.0]
        float _pad[3];               // 对齐填充
    };
}