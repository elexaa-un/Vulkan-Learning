// Vulkan学习项目 — 应用程序主类
// 统筹窗口、设备、渲染器和所有渲染系统的初始化、主循环和清理

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
#include "systems\lve_post_processing_system.hpp"
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
    // 应用程序主类：管理所有Vulkan资源和渲染系统的生命周期
    class FirstApp
    {

    public:
        static constexpr int WIDTH = 800;   // 默认窗口宽度
        static constexpr int HEIGHT = 600;  // 默认窗口高度

        FirstApp();
        ~FirstApp();

        FirstApp(const FirstApp &) = delete;
        FirstApp &operator=(const FirstApp &) = delete;

        // 主循环入口
        void run();

    private:
        // 风力参数：传递给植被系统的风力模拟数据
        struct WindParams
        {
            float strength = 0.5f;   // 风力强度
            float speed = 1.0f;      // 风速
            float totalTime = 0.0f;  // 累计时间（用于动画）
        };

        bool m_showDebugPanel = true;   // ImGui 调试面板显示开关
        bool m_debugKeyPressed = false; // 防止 F1 重复触发

        // 加载场景中的游戏对象
        void LoadGameObjects();

        // 初始化阶段拆分为独立函数
        void initGlobalResources();    // 初始化全局描述符集布局和池
        void initUboResources();       // 初始化全局UBO缓冲区和描述符集
        std::shared_ptr<LveTexture> initSkybox(); // 初始化天空盒纹理
        void initShadowResources(LveShadowSystem &shadowSystem); // 初始化阴影系统
        void initVegetationSystem(LveVegetationSystem &vegetationSystem, int instanceCount); // 初始化植被系统
        void initRenderSystems(std::unique_ptr<LveSimpleRenderSystem> &simpleRenderSystem,
                               std::unique_ptr<LvePointLightSystem> &pointLightSystem); // 初始化渲染系统

        // 核心Vulkan资源（声明顺序决定初始化/销毁顺序）
        LveWindow lveWindow{WIDTH, HEIGHT, "Vulkan Tutorial"}; // GLFW窗口
        LveDevice lveDevice{lveWindow};              // Vulkan逻辑设备
        LveRenderer lveRenderer{lveWindow, lveDevice}; // 渲染器（含交换链）
        LveResourceManager m_resourceManager{lveDevice}; // 统一资源管理器

        std::unique_ptr<LveDescriptorPool> globalPool{}; // 全局描述符池
        LveGameObject::Map gameObjects;                   // 场景中的游戏对象映射表

        // 材质系统描述符
        std::unique_ptr<LveDescriptorSetLayout> materialSetLayout;
        std::unique_ptr<LveDescriptorPool> materialPool;

        // 植被纹理描述符
        std::unique_ptr<LveDescriptorSetLayout> m_vegetationTextureLayout;
        std::unique_ptr<LveDescriptorPool> m_vegetationTexturePool;

        // 将 run() 中的局部变量提升为成员，统一生命周期管理
        std::unique_ptr<LveDescriptorSetLayout> globalSetLayout; // 全局描述符集布局
        std::vector<VkDescriptorSet> globalDescriptorSets;        // 飞行帧描述符集
        std::vector<std::unique_ptr<LveBuffer>> uboBuffers;       // 飞行帧UBO缓冲区
        std::shared_ptr<LveTexture> skyboxTexture;                // 天空盒纹理
        std::unique_ptr<LvePostProcessingSystem> m_postProcessingSystem; // 后处理系统
    };
}
