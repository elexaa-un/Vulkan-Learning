#include "first_app.hpp"
#include "lve_camera.hpp"
#include "lve_texture.hpp"
#include "systems\lve_simple_renderer_system.hpp"
#include "systems\lve_point_light_system.hpp"
#include "systems\lve_skybox_system.hpp"
#include "systems\lve_vegetation_system.hpp"
#include "keyboard_movement_controller.hpp"
#include "lve_model.hpp"
#include "terrain_height_sampler.hpp"
#include <stdexcept>
#include <array>
#include <iostream>
#include <cassert>
#include <chrono>
#include <numeric>
#define MAX_FRAME_TIME 2.0f
namespace lve
{

    FirstApp::FirstApp()
    {
        globalPool = LveDescriptorPool::Builder(lveDevice)
                         .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT * 4)                                             // 增加 set 总数（原为 3，改为 12）
                         .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT * 4)         // UBO 数量
                         .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LveSwapChain::MAX_FRAMES_IN_FLIGHT * 8) // 纹理 sampler（天空盒需要额外一个）
                         .build();

        materialSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
                                .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)         // 材质参数 UBO
                                .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // 漫反射贴图
                                .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // 法线贴图
                                .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // 高光贴图
                                .build();
        materialPool = LveDescriptorPool::Builder(lveDevice)
                           .setMaxSets(200)
                           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 200)
                           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 600)
                           .build();

        // 植物纹理描述符布局（set=1）
        m_vegetationTextureLayout = LveDescriptorSetLayout::Builder(lveDevice)
                                        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                        .build();

        // 为植物纹理分配描述符池（可以在全局池中预留，或单独创建）
        m_vegetationTexturePool = LveDescriptorPool::Builder(lveDevice)
                                      .setMaxSets(1)
                                      .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
                                      .build();
        LoadGameObjects();
    }
    void FirstApp::run()
    {
        VkDeviceSize alignment = std::max(lveDevice.properties.limits.minUniformBufferOffsetAlignment,
                                          lveDevice.properties.limits.nonCoherentAtomSize);
        std::vector<std::unique_ptr<LveBuffer>> uboBuffers(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < uboBuffers.size(); i++)
        {
            uboBuffers[i] = std::make_unique<LveBuffer>(
                lveDevice,
                sizeof(GlobalUbo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                alignment);
            uboBuffers[i]->map();
        }

        auto globalSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
                                   .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                                   .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .build();

        std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < globalDescriptorSets.size(); i++)
        {
            auto bufferInfo = uboBuffers[i]->descriptorInfo();
            VkDescriptorSet set;
            LveDescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &bufferInfo)
                .build(set);
            globalDescriptorSets[i] = set;
            if (set == VK_NULL_HANDLE)
            {
                throw std::runtime_error("Failed to allocate global descriptor set for frame " + std::to_string(i));
            }
        }

        //============================创建天空盒===========================
        auto skyboxTexture = LveTexture::createCubemap(lveDevice, {"E:/Vulkan-Learning/skybox/px.png",
                                                                   "E:/Vulkan-Learning/skybox/nx.png",
                                                                   "E:/Vulkan-Learning/skybox/py.png",
                                                                   "E:/Vulkan-Learning/skybox/ny.png",
                                                                   "E:/Vulkan-Learning/skybox/pz.png",
                                                                   "E:/Vulkan-Learning/skybox/nz.png"});

        // 为每个帧的描述符集写入天空盒纹理
        VkDescriptorImageInfo skyboxImageInfo{};
        skyboxImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        skyboxImageInfo.imageView = skyboxTexture->getImageView();
        skyboxImageInfo.sampler = skyboxTexture->getSampler();

        for (int i = 0; i < globalDescriptorSets.size(); ++i)
        {
            auto bufferInfo = uboBuffers[i]->descriptorInfo();

            LveDescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &bufferInfo)
                .writeImage(1, &skyboxImageInfo) // 写入纹理
                .build(globalDescriptorSets[i]);
        }
        auto skyboxModel = lve::createSkyboxModel(lveDevice);
        LveSkyboxSystem skyboxSystem(lveDevice, lveRenderer.GetSwapChainRenderPass(),
                                     globalSetLayout->getDescriptorSetLayout());

        // ========= 植物系统相关（新增） =========
        LveVegetationSystem vegetationSystem(lveDevice,
                                             lveRenderer.GetSwapChainRenderPass(),
                                             globalSetLayout->getDescriptorSetLayout(),
                                             "E:\\Vulkan-Learning\\tree.png");
        // ========= 创建实例数据 =========
        TerrainHeightSampler terrainSampler("E:\\Vulkan-Learning\\tt.jpg", 200.0f, 200.0f, -5.0f, 20.0f);
        std::vector<VegetationInstance> instances;
        // 填充实例数据（需要获取地形高度）
        for (int i = 0; i < 1000; ++i)
        {
            float x = (rand() % 200) - 100;
            float z = (rand() % 200) - 100;
            float y = terrainSampler.getHeight(x, z);
            instances.push_back({glm::vec3(x, y, z), 0.5f + (rand() % 100) / 100.0f});
        }
        vegetationSystem.createInstances(instances);

        //============================创建渲染系统===========================
        std::unique_ptr<LveSimpleRenderSystem> simpleRenderSystem;
        simpleRenderSystem = std::make_unique<LveSimpleRenderSystem>(
            lveDevice, lveRenderer.GetSwapChainRenderPass(),
            globalSetLayout->getDescriptorSetLayout(),
            materialSetLayout->getDescriptorSetLayout());

        std::unique_ptr<LvePointLightSystem> pointLightSystem;
        pointLightSystem = std::make_unique<LvePointLightSystem>(
            lveDevice, lveRenderer.GetSwapChainRenderPass(),
            globalSetLayout->getDescriptorSetLayout());
        LveCamera camera{};
        camera.setViewTarget(glm::vec3{1.f, 2.f, 20.f}, glm::vec3{.0f, .0f, 2.5f});

        auto viewerObject = LveGameObject::createGameObject();
        viewerObject.transform.translation.z = -2.5f;
        KeyBoardMovementController cameraController{};

        auto currentTime = std::chrono::high_resolution_clock::now();

        while (!lveWindow.shouldClose())
        {
            glfwPollEvents();

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            frameTime = glm::min(frameTime, MAX_FRAME_TIME);

            cameraController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, viewerObject);
            camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

            float aspect = lveRenderer.getAspectRatio();
            camera.setPerspectiveProject(glm::radians(50.0f), aspect, 0.1f, 100.f);
            // 关键：格式变化时重建渲染系统
            if (lveRenderer.hasFormatsChanged())
            {
                std::cout << "Formats changed, recreating render system..." << std::endl;
                vkDeviceWaitIdle(lveDevice.device());
                simpleRenderSystem = std::make_unique<LveSimpleRenderSystem>(
                    lveDevice, lveRenderer.GetSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout(), materialSetLayout->getDescriptorSetLayout());
                lveRenderer.resetFormatsChangedFlag();
            }

            if (auto commandBuffer = lveRenderer.beginFrame())
            {
                int frameIndex = lveRenderer.getFrameIndex();
                // 确保描述符集有效
                if (globalDescriptorSets[frameIndex] == VK_NULL_HANDLE)
                {
                    std::cerr << "Error: globalDescriptorSet is null!" << std::endl;
                    continue;
                }

                FrameInfo frameInfo{
                    frameIndex,
                    frameTime,
                    commandBuffer,
                    camera,
                    globalDescriptorSets[frameIndex],
                    gameObjects};

                GlobalUbo ubo{};
                ubo.projection = camera.getProjection();
                ubo.view = camera.getView();
                ubo.inverseView = camera.getInverseView();
                pointLightSystem->update(frameInfo, ubo);
                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();

                lveRenderer.beginSwapChainRenderPass(commandBuffer);
                skyboxSystem.render(frameInfo, skyboxTexture, *skyboxModel);
                simpleRenderSystem->rendererGameObjects(frameInfo);
                vegetationSystem.render(frameInfo);
                pointLightSystem->renderer(frameInfo);
                lveRenderer.endSwapChainRenderPass(commandBuffer);
                lveRenderer.endFrame();
            }
        }
        vkDeviceWaitIdle(lveDevice.device());
    }

    FirstApp::~FirstApp()
    {
    }

    void FirstApp::LoadGameObjects()
    {
        LveMaterialManager materialManager(*materialSetLayout, *materialPool, lveDevice);

        auto catMaterial = materialManager.createMaterial("apple");
        auto diffuseTex = std::make_shared<LveTexture>(lveDevice, "E:\\Vulkan-Learning\\apple\\image\\Model_0.jpg");
        catMaterial->setTexture(LveMaterial::TextureType::DIFFUSE, diffuseTex);

        catMaterial->setBaseColorFactor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        catMaterial->setMetallicFactor(0.2f);
        catMaterial->setRoughnessFactor(0.5f);

        catMaterial->buildDescriptorSet();
        auto gameObj = LveGameObject::createGameObject();
        gameObj.model = LveModel::createModelFromFile(lveDevice, "E:\\Vulkan-Learning\\apple\\Model.obj");
        gameObj.material = catMaterial; // 直接赋值 material 成员
        gameObj.transform.translation = {0.f, 0.f, 0.f};
        gameObj.transform.rotation = {0.f, 0.f, 1.f * 3.14f};
        gameObj.transform.scale = glm::vec3{0.02f};

        gameObjects.emplace(gameObj.getId(), std::move(gameObj));

        auto terrainModel = lve::createTerrainModel(lveDevice,
                                                    "E:\\Vulkan-Learning\\tt.jpg", // 你的高度图路径
                                                    200.0f,                        // 宽度 200
                                                    200.0f,                        // 深度 200
                                                    256,                           // 分段数 256 (顶点 257x257)
                                                    -5.0f,                         // 最低高度
                                                    20.0f,                         // 最高高度
                                                    "E:\\Vulkan-Learning\\tt.jpg");

        auto terrainMaterial = materialManager.createMaterial("terrain");
        auto terrainTex = std::make_shared<LveTexture>(lveDevice, "E:\\Vulkan-Learning\\tt.jpg");
        terrainMaterial->setTexture(LveMaterial::TextureType::DIFFUSE, terrainTex);
        terrainMaterial->setBaseColorFactor(glm::vec4(1.0f));
        terrainMaterial->setMetallicFactor(0.0f);
        terrainMaterial->setRoughnessFactor(0.8f);
        terrainMaterial->buildDescriptorSet();

        auto terrainObj = LveGameObject::createGameObject();
        terrainObj.model = std::move(terrainModel);
        terrainObj.material = terrainMaterial;
        terrainObj.transform.translation = {0.0f, 0.0f, 0.0f};
        gameObjects.emplace(terrainObj.getId(), std::move(terrainObj));

        {
            auto lightObj = LveGameObject::makePointLight(100.f, 10.f, glm::vec3{1.f, 1.f, 1.f});
            lightObj.transform.translation = {0.f, 20.f, 0.f};
            gameObjects.emplace(lightObj.getId(), std::move(lightObj));
        }
    }
}