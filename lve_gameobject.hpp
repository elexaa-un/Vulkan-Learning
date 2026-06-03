// Vulkan学习项目 — 游戏对象类
// 场景中所有可渲染实体的基础类，包含变换、材质、纹理和可选的点光源组件

#pragma once

#include "lve_model.hpp"
#include "lve_texture.hpp"
#include "lve_material.hpp"
#include <glm/gtc/matrix_transform.hpp>

#include <memory>

namespace lve
{
    // 游戏对象：表示场景中的一个可渲染实体
    class LveGameObject
    {
        // 变换组件：包含位置、旋转、缩放的Transform数据
        struct TransformComponent
        {
            glm::vec3 translation{};           // 世界空间平移
            glm::vec3 scale{1.f, 1.f, 1.f};    // 各轴缩放
            glm::vec3 rotation;                 // 旋转角度（Tait-Bryan角：Y, X, Z）
            // 计算4x4模型矩阵：Translate * Ry * Rx * Rz * Scale
            // 旋转顺序对应Tait-Bryan角：Y(1), X(2), Z(3)
            // https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
            glm::mat4 mat4();
            // 计算3x3法线矩阵（模型矩阵的逆转置，用于法线变换）
            glm::mat3 normalMartix();
        };

        // 点光源组件：可选组件，使该对象成为一个点光源
        struct PointLightComponent
        {
            float lightIntensity = 1.0f; // 光源强度乘数
        };

    public:
        using id_t = unsigned int;                          // 游戏对象唯一ID类型
        using Map = std::unordered_map<id_t, LveGameObject>; // 游戏对象映射表

        // 禁止拷贝（每个游戏对象有唯一ID），允许移动
        LveGameObject(const LveGameObject &) = delete;
        LveGameObject &operator=(const LveGameObject &) = delete;
        LveGameObject(LveGameObject &&) = default;
        LveGameObject &operator=(LveGameObject &&) = default;

        // 工厂方法：创建一个带有点光源组件的游戏对象
        // 执行步骤：
        //   1. 调用 createGameObject() 创建基础对象
        //   2. 设置 color 为光源颜色
        //   3. 创建 PointLightComponent 并设置强度
        //   4. 为光源创建球形模型（使用半径）
        static LveGameObject makePointLight(
            float intensity = 10.f, float radius = 0.1f, glm::vec3 color = glm::vec3{1.f});

        // 工厂方法：创建一个带有自增唯一ID的游戏对象
        static LveGameObject createGameObject()
        {
            static id_t currentId = 0;
            return LveGameObject{currentId++};
        }

        id_t getId() const { return id; }
        void setMaterial(std::shared_ptr<LveMaterial> mat) { material = mat; }
        std::shared_ptr<LveMaterial> getMaterial() const { return material; }
        glm::vec3 color{};                              // 对象颜色（用于光源等）
        TransformComponent transform{};                  // 变换组件

        std::shared_ptr<LveModel> model{};               // 关联的3D模型
        std::vector<std::shared_ptr<LveTexture>> textures; // 关联的纹理列表
        VkDescriptorSet materialDescriptorSet = VK_NULL_HANDLE; // 材质描述符集
        std::unique_ptr<PointLightComponent> pointLight = nullptr; // 可选的点光源组件

    private:
        LveGameObject() : id(0), material(nullptr) {}    // 默认构造（ID=0）
        LveGameObject(id_t objId) : id(objId) {}          // 指定ID构造
        std::shared_ptr<LveMaterial> material;            // 材质指针
        id_t id;                                          // 唯一标识符
    };

}
