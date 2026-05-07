// vegetation_instance.hpp
#pragma once
#include <glm/glm.hpp>

namespace lve
{
    struct VegetationInstance
    {
        glm::vec3 position{0.0f};
        float scale{1.0f};
    };
}