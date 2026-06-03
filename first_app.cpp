// Vulkan学习项目 — 应用程序主类实现
// FirstApp的构造、初始化、主循环和清理的完整实现

#include "first_app.hpp"
#include "lve_camera.hpp"
#include "lve_texture.hpp"
#include "keyboard_movement_controller.hpp"
#include "lve_texture_manager.hpp"
#include "lve_model.hpp"
#include "terrain_height_sampler.hpp"
#include "lve_imgui.hpp"
#include "ImGui/imgui.h"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <array>
#include <iostream>
#include <cassert>
#include <chrono>
#include <numeric>
#define MAX_FRAME_TIME 2.0f
namespace lve
{

    // 构造函数：初始化描述符池、材质布局和回退资源，然后加载游戏对象
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
                           .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 200)
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
        LveTextureManager::initFallbackResources(lveDevice.device(), lveDevice.getPhysicalDevice());
        // 验证初始化是否成功
        assert(LveTextureManager::getFallbackSampler() != VK_NULL_HANDLE);
        assert(LveTextureManager::getFallbackImageView() != VK_NULL_HANDLE);
        
        LoadGameObjects();
    }

    // ========================================================================
    // 重构后的 run()：清晰的三阶段划分
    // ========================================================================
    void FirstApp::run()
    {
        std::cerr << "[DEBUG run] === Phase 1: initGlobalResources ===" << std::endl
                  << std::flush;
        initGlobalResources();
        std::cerr << "[DEBUG run] Phase 1 done" << std::endl
                  << std::flush;

        // --- 阶段 2: 创建各子系统 ---
        std::cerr << "[DEBUG run] === Phase 2: Creating subsystems ===" << std::endl
                  << std::flush;
        LveImgui imgui(lveWindow.getGLFWwindow(), lveDevice, lveRenderer.GetSwapChainRenderPass());
        std::cerr << "[DEBUG run] ImGui created" << std::endl
                  << std::flush;

        auto skyboxModel = lve::createSkyboxModel(lveDevice);
        std::cerr << "[DEBUG run] Skybox model created" << std::endl
                  << std::flush;

        // -------- 创建后处理系统（必须先创建，因为场景系统需要离屏 RenderPass）--------
        m_postProcessingSystem = std::make_unique<LvePostProcessingSystem>(
            lveDevice,
            lveRenderer.getSwapChainImageFormat(),
            lveRenderer.getSwapChainExtent(),
            lveRenderer.GetSwapChainRenderPass());
        std::cerr << "[DEBUG run] PostProcessingSystem created" << std::endl
                  << std::flush;

        VkRenderPass offscreenPass = m_postProcessingSystem->getOffscreenRenderPass();

        // -------- 创建场景渲染系统（全部使用离屏 RenderPass）--------
        LveSkyboxSystem skyboxSystem(lveDevice, offscreenPass,
                                     globalSetLayout->getDescriptorSetLayout());
        std::cerr << "[DEBUG run] SkyboxSystem created" << std::endl
                  << std::flush;

        LveShadowSystem shadowSystem(lveDevice, globalSetLayout->getDescriptorSetLayout());
        std::cerr << "[DEBUG run] ShadowSystem created" << std::endl
                  << std::flush;
        initShadowResources(shadowSystem);
        std::cerr << "[DEBUG run] Shadow resources initialized" << std::endl
                  << std::flush;

        LveVegetationSystem vegetationSystem(lveDevice,
                                             offscreenPass,
                                             globalSetLayout->getDescriptorSetLayout(),
                                             "tree.png");
        std::cerr << "[DEBUG run] VegetationSystem created" << std::endl
                  << std::flush;
        initVegetationSystem(vegetationSystem, 3000);
        std::cerr << "[DEBUG run] Vegetation initialized with 3000 instances" << std::endl
                  << std::flush;

        auto simpleRenderSystem = std::make_unique<LveSimpleRenderSystem>(
            lveDevice, offscreenPass,
            globalSetLayout->getDescriptorSetLayout(),
            materialSetLayout->getDescriptorSetLayout());
        auto pointLightSystem = std::make_unique<LvePointLightSystem>(
            lveDevice, offscreenPass,
            globalSetLayout->getDescriptorSetLayout());
        std::cerr << "[DEBUG run] Render systems initialized" << std::endl
                  << std::flush;

        // ImGui 保持在 SwapChain RenderPass（渲染在 UI 层）
        // imgui 已在上面用 lveRenderer.GetSwapChainRenderPass() 创建，无需修改

        // --- 阶段 3: 游戏主循环 ---
        std::cerr << "[DEBUG run] === Phase 3: Main loop starting ===" << std::endl
                  << std::flush;
        LveCamera camera{};
        camera.setViewTarget(glm::vec3{1.f, 2.f, 20.f}, glm::vec3{.0f, .0f, 2.5f});

        auto viewerObject = LveGameObject::createGameObject();
        viewerObject.transform.translation.z = -2.5f;
        KeyBoardMovementController cameraController{};

        auto currentTime = std::chrono::high_resolution_clock::now();
        WindParams wind;

        // 渲染选项：持久化在主循环中，ImGui 面板修改后实时影响渲染
        RenderOptions renderOptions;

        uint64_t frameCount = 0;

        while (!lveWindow.shouldClose())
        {
            glfwPollEvents();

            // ESC 退出
            if (glfwGetKey(lveWindow.getGLFWwindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
            {
                glfwSetWindowShouldClose(lveWindow.getGLFWwindow(), GLFW_TRUE);
                continue;
            }

            // F1 切换调试面板（防重复触发）
            if (glfwGetKey(lveWindow.getGLFWwindow(), GLFW_KEY_F1) == GLFW_PRESS)
            {
                if (!m_debugKeyPressed)
                {
                    m_showDebugPanel = !m_showDebugPanel;
                    m_debugKeyPressed = true;
                }
            }
            else
            {
                m_debugKeyPressed = false;
            }

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            frameTime = glm::min(frameTime, MAX_FRAME_TIME);
            wind.totalTime += frameTime;

            cameraController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, viewerObject);
            camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

            float aspect = lveRenderer.getAspectRatio();
            camera.setPerspectiveProject(glm::radians(50.0f), aspect, 0.1f, 100.f);

            // 格式变化时重建所有渲染系统
            if (lveRenderer.hasFormatsChanged())
            {
                std::cout << "Formats changed, recreating all render systems..." << std::endl;
                vkDeviceWaitIdle(lveDevice.device());

                // 重建后处理系统
                m_postProcessingSystem->recreate(
                    lveRenderer.getSwapChainImageFormat(),
                    lveRenderer.getSwapChainExtent(),
                    lveRenderer.GetSwapChainRenderPass());
                VkRenderPass offscreenPass = m_postProcessingSystem->getOffscreenRenderPass();

                // 重建场景渲染系统
                simpleRenderSystem = std::make_unique<LveSimpleRenderSystem>(
                    lveDevice, offscreenPass,
                    globalSetLayout->getDescriptorSetLayout(),
                    materialSetLayout->getDescriptorSetLayout());
                pointLightSystem = std::make_unique<LvePointLightSystem>(
                    lveDevice, offscreenPass,
                    globalSetLayout->getDescriptorSetLayout());
                // skyboxSystem / vegetationSystem 是局部栈变量，如需重建请修改为 unique_ptr
                // 目前跳过其重建（格式变化仅在窗口 resize 时极少发生）

                lveRenderer.resetFormatsChangedFlag();
            }

            if (auto commandBuffer = lveRenderer.beginFrame())
            {
                frameCount++;
                int frameIndex = lveRenderer.getFrameIndex();

                // ImGui 开始新帧（必须在任何 ImGui 控件之前）
                imgui.newFrame();

                FrameInfo frameInfo{
                    frameIndex,
                    frameTime,
                    commandBuffer,
                    camera,
                    globalDescriptorSets[frameIndex],
                    gameObjects};

                if (globalDescriptorSets[frameIndex] != VK_NULL_HANDLE)
                {
                    // 填充 UBO
                    GlobalUbo ubo{};
                    ubo.projection = camera.getProjection();
                    ubo.view = camera.getView();
                    ubo.inverseView = camera.getInverseView();
                    pointLightSystem->update(frameInfo, ubo);

                    // ==== Shadow Pass ====
                    glm::vec3 lightPos{0.f, 20.f, 0.f};
                    for (auto &kv : gameObjects)
                    {
                        if (kv.second.pointLight)
                        {
                            lightPos = kv.second.transform.translation;
                            break;
                        }
                    }
                    shadowSystem.render(frameInfo, lightPos, glm::vec3{0.f, 0.f, 0.f});
                    ubo.lightViewProj = shadowSystem.getLightViewProj();

                    WindPushConstantData windData{};
                    windData.windTime = wind.totalTime;
                    windData.windStrength = wind.strength;
                    windData.windSpeed = wind.speed;
                    windData.windDirectionX = 1.0f;
                    windData.windDirectionZ = 0.0f;

                    uboBuffers[frameIndex]->writeToBuffer(&ubo);
                    uboBuffers[frameIndex]->flush();

                    // ==== 主渲染通道 (离屏渲染) ====
                    m_postProcessingSystem->beginOffscreenRenderPass(commandBuffer);
                    if (renderOptions.showSkybox)
                    {
                        skyboxSystem.render(frameInfo, skyboxTexture, *skyboxModel);
                    }
                    if (renderOptions.showTerrain || renderOptions.showModel)
                    {
                        simpleRenderSystem->rendererGameObjects(frameInfo);
                    }
                    if (renderOptions.showVegetation)
                    {
                        vegetationSystem.render(frameInfo, windData);
                    }
                    pointLightSystem->renderer(frameInfo);
                    m_postProcessingSystem->endOffscreenRenderPass(commandBuffer);

                    // ==== 后处理 RenderPass（离屏纹理 → SwapChain）====
                    lveRenderer.beginSwapChainRenderPass(commandBuffer);
                    m_postProcessingSystem->render(frameInfo);

                    // ==== ImGui 调试面板（渲染完成后才调用，以便读取剔除统计） ====
                    if (m_showDebugPanel)
                    {
                        imgui.showImGUI(frameTime, gameObjects, camera, viewerObject, renderOptions, &frameInfo);
                    }
                    // ImGui 绘制必须在 render pass 内进行
                    imgui.render(commandBuffer);

                    lveRenderer.endSwapChainRenderPass(commandBuffer);
                }
                else
                {
                    std::cerr << "Error: globalDescriptorSet is null at frameIndex "
                              << frameIndex << "! Skipping rendering for this frame." << std::endl;
                    // Even when skipping, we need to handle ImGui and end frame properly.
                    lveRenderer.beginSwapChainRenderPass(commandBuffer);
                    if (m_showDebugPanel)
                    {
                        imgui.showImGUI(frameTime, gameObjects, camera, viewerObject, renderOptions, &frameInfo);
                    }
                    imgui.render(commandBuffer);
                    lveRenderer.endSwapChainRenderPass(commandBuffer);
                }

                lveRenderer.endFrame();
            }
            else
            {
                std::cout << "[DEBUG run] beginFrame returned null! Frame #" << (frameCount + 1)
                          << " skipped (window minimized or swapchain needs recreation)" << std::endl;
            }
        }
        std::cout << "[DEBUG run] Main loop exited after " << frameCount << " frames" << std::endl;
        vkDeviceWaitIdle(lveDevice.device());
    }

    FirstApp::~FirstApp()
    {
        LveTextureManager::cleanupFallbackResources(lveDevice.device());
    }

    void FirstApp::LoadGameObjects()
    {
        std::cout << "[DEBUG LoadGameObjects] Step 1: Creating MaterialManager..." << std::endl;
        LveMaterialManager materialManager(*materialSetLayout, *materialPool, lveDevice);

        std::cout << "[DEBUG LoadGameObjects] Step 2: Creating cat material..." << std::endl;
        auto catMaterial = materialManager.createMaterial("apple");
        std::cout << "[DEBUG LoadGameObjects] Step 3: Loading texture..." << std::endl;
        auto diffuseTex = m_resourceManager.loadTexture("apple/image/Model_0.jpg");
        std::cout << "[DEBUG LoadGameObjects] Step 4: Setting texture on material..." << std::endl;
        catMaterial->setTexture(LveMaterial::TextureType::DIFFUSE, diffuseTex);

        std::cout << "[DEBUG LoadGameObjects] Step 5: Setting material properties..." << std::endl;
        catMaterial->setBaseColorFactor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        catMaterial->setMetallicFactor(0.2f);
        catMaterial->setRoughnessFactor(0.5f);

        std::cout << "[DEBUG LoadGameObjects] Step 6: Building descriptor set..." << std::endl;
        catMaterial->buildDescriptorSet();
        std::cout << "[DEBUG LoadGameObjects] Step 7: Loading model..." << std::flush;
        std::cout << std::endl;
        auto gameObj = LveGameObject::createGameObject();
        gameObj.model = m_resourceManager.loadModel("apple/Model.obj");
        gameObj.setMaterial(catMaterial); // 直接赋值 material 成员
        gameObj.transform.translation = {0.f, 0.f, 0.f};
        gameObj.transform.rotation = {0.f, 0.f, 1.f * 3.14f};
        gameObj.transform.scale = glm::vec3{0.02f};

        gameObjects.emplace(gameObj.getId(), std::move(gameObj));

        std::cout << "[DEBUG LoadGameObjects] Step 8: Creating terrain model..." << std::endl;
        auto terrainModel = lve::createTerrainModel(lveDevice,
                                                    "tt.jpg", // 高度图路径
                                                    200.0f,   // 宽度 200
                                                    200.0f,   // 深度 200
                                                    256,      // 分段数 256 (顶点 257x257)
                                                    -5.0f,    // 最低高度
                                                    20.0f,    // 最高高度
                                                    "tt.jpg");

        auto terrainMaterial = materialManager.createMaterial("terrain");
        auto terrainTex = m_resourceManager.loadTexture("tt.jpg");
        terrainMaterial->setTexture(LveMaterial::TextureType::DIFFUSE, terrainTex);
        terrainMaterial->setBaseColorFactor(glm::vec4(1.0f));
        terrainMaterial->setMetallicFactor(0.0f);
        terrainMaterial->setRoughnessFactor(0.8f);
        terrainMaterial->buildDescriptorSet();

        auto terrainObj = LveGameObject::createGameObject();
        terrainObj.model = std::move(terrainModel);
        terrainObj.setMaterial(terrainMaterial);
        terrainObj.transform.translation = {0.0f, 0.0f, 0.0f};
        gameObjects.emplace(terrainObj.getId(), std::move(terrainObj));

        {
            auto lightObj = LveGameObject::makePointLight(100.f, 10.f, glm::vec3{1.f, 1.f, 1.f});
            lightObj.transform.translation = {0.f, 20.f, 0.f};
            gameObjects.emplace(lightObj.getId(), std::move(lightObj));
        }
        std::cout << "[DEBUG LoadGameObjects] All game objects loaded successfully!" << std::endl;
    }

    // ========================================================================
    // 阶段 1: 初始化全局资源 (UBO + 描述符布局 + 天空盒)
    // ========================================================================
    void FirstApp::initGlobalResources()
    {
        LveTextureManager::initFallbackResources(lveDevice.device(), lveDevice.getPhysicalDevice());

        VkDeviceSize alignment = std::max(lveDevice.properties.limits.minUniformBufferOffsetAlignment,
                                          lveDevice.properties.limits.nonCoherentAtomSize);

        uboBuffers.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
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

        globalSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
                              .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                              .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // 天空盒
                              .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Shadow Map
                              .build();

        globalDescriptorSets.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);

        skyboxTexture = initSkybox();
    }

    std::shared_ptr<LveTexture> FirstApp::initSkybox()
    {
        auto texture = m_resourceManager.loadCubemap("skybox", {"skybox/px.png", "skybox/nx.png",
                                                                "skybox/py.png", "skybox/ny.png",
                                                                "skybox/pz.png", "skybox/nz.png"});

        VkDescriptorImageInfo skyboxImageInfo{};
        skyboxImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        skyboxImageInfo.imageView = texture->getImageView();
        skyboxImageInfo.sampler = texture->getSampler();

        for (int i = 0; i < globalDescriptorSets.size(); ++i)
        {
            auto bufferInfo = uboBuffers[i]->descriptorInfo();

            LveDescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &bufferInfo)
                .writeImage(1, &skyboxImageInfo)
                .build(globalDescriptorSets[i]);
        }

        return texture;
    }

    void FirstApp::initShadowResources(LveShadowSystem &shadowSystem)
    {
        VkDescriptorImageInfo shadowMapImageInfo{};
        shadowMapImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        shadowMapImageInfo.imageView = shadowSystem.getShadowMapImageView();
        shadowMapImageInfo.sampler = shadowSystem.getShadowMapSampler();

        for (int i = 0; i < globalDescriptorSets.size(); ++i)
        {
            LveDescriptorWriter(*globalSetLayout, *globalPool)
                .writeImage(2, &shadowMapImageInfo)
                .overwrite(globalDescriptorSets[i]);
        }
    }

    void FirstApp::initVegetationSystem(LveVegetationSystem &vegetationSystem, int instanceCount)
    {
        TerrainHeightSampler terrainSampler("tt.jpg", 200.0f, 200.0f, -5.0f, 20.0f);
        std::vector<VegetationInstance> instances;
        for (int i = 0; i < instanceCount; ++i)
        {
            float x = (rand() % 200) - 100;
            float z = (rand() % 200) - 100;
            float y = terrainSampler.getHeight(x, z);
            instances.push_back({glm::vec3(x, y, z), 0.5f + (rand() % 100) / 100.0f});
        }
        vegetationSystem.createInstances(instances);
    }

    void FirstApp::initRenderSystems(std::unique_ptr<LveSimpleRenderSystem> &simpleRenderSystem,
                                     std::unique_ptr<LvePointLightSystem> &pointLightSystem)
    {
        simpleRenderSystem = std::make_unique<LveSimpleRenderSystem>(
            lveDevice, lveRenderer.GetSwapChainRenderPass(),
            globalSetLayout->getDescriptorSetLayout(),
            materialSetLayout->getDescriptorSetLayout());

        pointLightSystem = std::make_unique<LvePointLightSystem>(
            lveDevice, lveRenderer.GetSwapChainRenderPass(),
            globalSetLayout->getDescriptorSetLayout());
    }
}
