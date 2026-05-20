#pragma once

#include "lve_window.hpp"
#include "lve_device.hpp"
#include "lve_renderer.hpp"
#include "lve_gameobject.hpp"
#include "lve_buffer.hpp"
#include "lve_descriptors.hpp"
#include "systems\lve_simple_renderer_system.hpp"
#include "systems\lve_point_light_system.hpp"
#include "systems\lve_skybox_system.hpp"
#include "systems\lve_vegetation_system.hpp"
#include "systems\lve_shadow_system.hpp"
#include "lve_resource_manager.hpp"
#include "lve_texture_manager.hpp"
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
        // ===== 风力参数 =====
        struct WindParams
        {
            float strength = 0.5f;
            float speed = 1.0f;
            float totalTime = 0.0f;
        };

        bool m_showDebugPanel = true;   // ImGui 调试面板显示开关
        bool m_debugKeyPressed = false; // 防止 F1 重复触发

        void LoadGameObjects();

        // 重构：将 run() 中的初始化拆分为独立阶段
        void initGlobalResources();
        void initUboResources();
        std::shared_ptr<LveTexture> initSkybox();
        void initShadowResources(LveShadowSystem &shadowSystem);
        void initVegetationSystem(LveVegetationSystem &vegetationSystem, int instanceCount);
        void initRenderSystems(std::unique_ptr<LveSimpleRenderSystem> &simpleRenderSystem,
                               std::unique_ptr<LvePointLightSystem> &pointLightSystem);

        LveWindow lveWindow{WIDTH, HEIGHT, "Vulkan Tutorial"};
        LveDevice lveDevice{lveWindow};
        LveRenderer lveRenderer{lveWindow, lveDevice};
        LveResourceManager m_resourceManager{lveDevice}; // 新增：统一资源管理器

        // note: order of declarations matters
        std::unique_ptr<LveDescriptorPool> globalPool{};
        LveGameObject::Map gameObjects;

        std::unique_ptr<LveDescriptorSetLayout> materialSetLayout;
        std::unique_ptr<LveDescriptorPool> materialPool;

        std::unique_ptr<LveDescriptorSetLayout> m_vegetationTextureLayout;
        std::unique_ptr<LveDescriptorPool> m_vegetationTexturePool;

        // 重构：将 run() 中的局部变量提升为成员，统一生命周期管理
        std::unique_ptr<LveDescriptorSetLayout> globalSetLayout;
        std::vector<VkDescriptorSet> globalDescriptorSets;
        std::vector<std::unique_ptr<LveBuffer>> uboBuffers;
        std::shared_ptr<LveTexture> skyboxTexture;
    };
}
