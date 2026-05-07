#pragma once

#include "lve_model.hpp"
#include "lve_texture.hpp"
#include "lve_material.hpp"
#include <glm/gtc/matrix_transform.hpp>

#include <memory>

namespace lve
{
    class LveGameObject
    {
        struct TransformComponent
        {
            glm::vec3 translation{};
            glm::vec3 scale{1.f, 1.f, 1.f};
            glm::vec3 rotation;
            // Matrix corrsponds to Translate * Ry * Rx * Rz * Scale
            // Rotations correspond to Tait-bryan angles of Y(1), X(2), Z(3)
            // https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
            glm::mat4 mat4();
            glm::mat3 normalMartix();
        };

        struct PointLightComponent
        {
            float lightIntensity = 1.0f;
        };

    public:
        using id_t = unsigned int;
        using Map = std::unordered_map<id_t, LveGameObject>;

        LveGameObject(const LveGameObject &) = delete;
        LveGameObject &operator=(const LveGameObject &) = delete;
        LveGameObject(LveGameObject &&) = default;
        LveGameObject &operator=(LveGameObject &&) = default;

        static LveGameObject makePointLight(
            float intensity = 10.f, float radius = 0.1f, glm::vec3 color = glm::vec3{1.f});
        static LveGameObject createGameObject()
        {
            static id_t currentId = 0;
            return LveGameObject{currentId++};
        }

        id_t getId() const { return id; }
        void setMaterial(std::shared_ptr<LveMaterial> material);
        std::shared_ptr<LveMaterial> getMaterial() const { return m_material; }

        glm::vec3 color{};
        TransformComponent transform{};

        std::shared_ptr<LveModel> model{};
        std::shared_ptr<LveMaterial> material;
        std::vector<std::shared_ptr<LveTexture>> textures;
        VkDescriptorSet materialDescriptorSet = VK_NULL_HANDLE;
        std::unique_ptr<PointLightComponent> pointLight = nullptr;

    private:
        LveGameObject(id_t objId) : id(objId) {}
        std::shared_ptr<LveMaterial> m_material;
        id_t id;
    };

}