#pragma once

#include "lve_window.hpp"
#include "lve_device.hpp"
#include "lve_renderer.hpp"
#include "lve_gameobject.hpp"
#include "lve_buffer.hpp"
#include "lve_descriptors.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <memory>
#include <vector>
namespace lve
{

    class FirstApp
    {

    public:
        static constexpr int WIDTH = 800;
        static constexpr int HEIGHT = 600;

        FirstApp();
        ~FirstApp();

        FirstApp(const FirstApp &) = delete;
        FirstApp &operator=(const FirstApp &) = delete;
        void run();

    private:
        void LoadGameObjects();

        LveWindow lveWindow{WIDTH, HEIGHT, "Vulkan Tutorial"};
        LveDevice lveDevice{lveWindow};
        LveRenderer lveRenderer{lveWindow, lveDevice};

        // note: order of declarations matters
        std::unique_ptr<LveDescriptorPool> globalPool{};
        LveGameObject::Map gameObjects;

        std::unique_ptr<LveDescriptorSetLayout> materialSetLayout;
        std::unique_ptr<LveDescriptorPool> materialPool;

        std::unique_ptr<LveDescriptorSetLayout> m_vegetationTextureLayout;
        std::unique_ptr<LveDescriptorPool> m_vegetationTexturePool;
    };
}