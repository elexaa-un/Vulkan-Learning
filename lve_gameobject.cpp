// Vulkan学习项目 — 游戏对象实现
// Transform矩阵计算（模型矩阵和法线矩阵）和点光源工厂方法

#include "lve_gameobject.hpp"

namespace lve
{
    // 计算4x4模型矩阵
    // 顺序：Scale → Rz → Rx → Ry → Translation
    // 使用Tait-Bryan角（Y, X, Z顺序），公式来自维基百科旋转矩阵
    glm::mat4 LveGameObject::TransformComponent::mat4()
    {
        const float c3 = glm::cos(rotation.z);
        const float s3 = glm::sin(rotation.z);
        const float c2 = glm::cos(rotation.x);
        const float s2 = glm::sin(rotation.x);
        const float c1 = glm::cos(rotation.y);
        const float s1 = glm::sin(rotation.y);
        return glm::mat4{
            {
                scale.x * (c1 * c3 + s1 * s2 * s3),
                scale.x * (c2 * s3),
                scale.x * (c1 * s2 * s3 - c3 * s1),
                0.0f,
            },
            {
                scale.y * (c3 * s1 * s2 - c1 * s3),
                scale.y * (c2 * c3),
                scale.y * (c1 * c3 * s2 + s1 * s3),
                0.0f,
            },
            {
                scale.z * (c2 * s1),
                scale.z * (-s2),
                scale.z * (c1 * c2),
                0.0f,
            },
            {translation.x, translation.y, translation.z, 1.0f}};
    }

    // 计算3x3法线矩阵（模型矩阵旋转部分的逆转置，使用逆缩放因子）
    // 用于将法线从局部空间变换到世界空间
    glm::mat3 LveGameObject::TransformComponent::normalMartix()
    {
        const float c3 = glm::cos(rotation.z);
        const float s3 = glm::sin(rotation.z);
        const float c2 = glm::cos(rotation.x);
        const float s2 = glm::sin(rotation.x);
        const float c1 = glm::cos(rotation.y);
        const float s1 = glm::sin(rotation.y);

        const glm::vec3 invScale = glm::vec3(1.0f) / scale;
        return glm::mat3{
            {
                invScale.x * (c1 * c3 + s1 * s2 * s3),
                invScale.x * (c2 * s3),
                invScale.x * (c1 * s2 * s3 - c3 * s1),
            },
            {
                invScale.y * (c3 * s1 * s2 - c1 * s3),
                invScale.y * (c2 * c3),
                invScale.y * (c1 * c3 * s2 + s1 * s3),
            },
            {
                invScale.z * (c2 * s1),
                invScale.z * (-s2),
                invScale.z * (c1 * c2),
            },
        };
    }

    // 工厂方法：创建点光源游戏对象
    // 执行步骤：
    //   1. 调用createGameObject()获取自增ID的基础对象
    //   2. 设置color为光源颜色
    //   3. 通过scale.x存储光源半径
    //   4. 创建PointLightComponent并设置强度
    LveGameObject LveGameObject::makePointLight(
        float intensity, float radius, glm::vec3 color)
    {
        LveGameObject gameObj = LveGameObject::createGameObject();
        gameObj.color = color;
        gameObj.transform.scale.x = radius;
        gameObj.pointLight = std::make_unique<PointLightComponent>();
        gameObj.pointLight->lightIntensity = intensity;
        return gameObj;
    }
}
