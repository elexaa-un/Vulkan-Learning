#include "first_app.hpp"
#include "lve_camera.hpp"
#include "lve_texture.hpp"
#include "systems\lve_simple_renderer_system.hpp"
#include "systems\lve_point_light_system.hpp"
#include "systems\lve_skybox_system.hpp"
#include "systems\lve_vegetation_system.hpp"
#include "systems/lve_shadow_system.hpp"
#include "keyboard_movement_controller.hpp"
#include "lve_model.hpp"
#include "terrain_height_sampler.hpp"
#include "lve_imgui.hpp"
#include "ImGui/imgui.h"
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
                                   .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)  // 天空盒
                                   .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)  // Shadow Map
                                   .build();

        std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);

        //============================创建 ImGui 系统===========================
        LveImgui imgui(lveWindow.getGLFWwindow(), lveDevice, lveRenderer.GetSwapChainRenderPass());
        //============================创建天空盒===========================
        auto skyboxTexture = LveTexture::createCubemap(lveDevice, {"skybox/px.png",
                                                                   "skybox/nx.png",
                                                                   "skybox/py.png",
                                                                   "skybox/ny.png",
                                                                   "skybox/pz.png",
                                                                   "skybox/nz.png"});

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
                .writeImage(1, &skyboxImageInfo) // 写入天空盒纹理
                .build(globalDescriptorSets[i]);
        }
        // ============================================================
        // 创建 Shadow System（在渲染系统之前创建）
        // ============================================================
        LveShadowSystem shadowSystem(lveDevice, globalSetLayout->getDescriptorSetLayout());

        // 为每个帧的描述符集写入 Shadow Map
        VkDescriptorImageInfo shadowMapImageInfo{};
        shadowMapImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        shadowMapImageInfo.imageView = shadowSystem.getShadowMapImageView();
        shadowMapImageInfo.sampler = shadowSystem.getShadowMapSampler();

        for (int i = 0; i < globalDescriptorSets.size(); ++i)
        {
            LveDescriptorWriter(*globalSetLayout, *globalPool)
                .writeImage(2, &shadowMapImageInfo)  // binding 2 = Shadow Map
                .overwrite(globalDescriptorSets[i]); // 更新已有的描述符集，只写入 binding 2
        }

        auto skyboxModel = lve::createSkyboxModel(lveDevice);
        LveSkyboxSystem skyboxSystem(lveDevice, lveRenderer.GetSwapChainRenderPass(),
                                     globalSetLayout->getDescriptorSetLayout());

        // ========= 植物系统相关（新增） =========
        LveVegetationSystem vegetationSystem(lveDevice,
                                             lveRenderer.GetSwapChainRenderPass(),
                                             globalSetLayout->getDescriptorSetLayout(),
                                             "tree.png");
        // ========= 创建实例数据 =========
        TerrainHeightSampler terrainSampler("tt.jpg", 200.0f, 200.0f, -5.0f, 20.0f);
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

        // ==== 风力参数（声明在 ImGui 作用域外，以便写入 UBO）====
        float windStrength = 0.5f;
        float windSpeed = 1.0f;
        float totalTime = 0.0f; // 累计时间

        while (!lveWindow.shouldClose())
        {
            glfwPollEvents();

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            frameTime = glm::min(frameTime, MAX_FRAME_TIME);
            totalTime += frameTime; // 累计时间

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
                //====================imgui界面设置=====================
                imgui.newFrame();

                // ============================================================
                // 调试面板
                // ============================================================
                ImGui::Begin("Vulkan Renderer - Debug Panel");

                // ---------- Performance ----------
                if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Text("FPS: %.1f", 1.0f / frameTime);
                    ImGui::Text("Frame Time: %.2f ms", frameTime * 1000.0f);
                    ImGui::Text("Scene Objects: %d", (int)gameObjects.size());
                }

                // ---------- Camera ----------
                if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto &pos = viewerObject.transform.translation;
                    auto &rot = viewerObject.transform.rotation;
                    ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
                    ImGui::Text("Rotation: (%.2f, %.2f, %.2f)", rot.x, rot.y, rot.z);

                    // Quick position buttons
                    if (ImGui::Button("Reset (0,0,3)"))
                        viewerObject.transform.translation = glm::vec3{0.f, 0.f, 3.f};
                    ImGui::SameLine();
                    if (ImGui::Button("Top View (0,30,0)"))
                        viewerObject.transform.translation = glm::vec3{0.f, 30.f, 0.f};
                }

                // ---------- Lighting ----------
                if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    static float lightPos[3] = {0.f, 20.f, 0.f};
                    static float lightIntensity = 100.f;
                    static float lightRadius = 10.f;
                    static float lightColor[3] = {1.f, 1.f, 1.f};

                    if (ImGui::SliderFloat3("Light Position", lightPos, -50.f, 50.f))
                    {
                        // Update point light in scene
                        for (auto &kv : gameObjects)
                        {
                            if (kv.second.pointLight)
                            {
                                kv.second.transform.translation.x = lightPos[0];
                                kv.second.transform.translation.y = lightPos[1];
                                kv.second.transform.translation.z = lightPos[2];
                                kv.second.pointLight->lightIntensity = lightIntensity;
                            }
                        }
                    }
                    ImGui::SliderFloat("Intensity", &lightIntensity, 1.f, 500.f);
                    ImGui::SliderFloat("Radius", &lightRadius, 1.f, 50.f);
                    ImGui::ColorEdit3("Color", lightColor);
                }

                // ---------- Render Options ----------
                if (ImGui::CollapsingHeader("Render Options"))
                {
                    static bool showSkybox = true;
                    static bool showTerrain = true;
                    static bool showVegetation = true;
                    static bool showModel = true;
                    static bool wireframe = false;

                    ImGui::Checkbox("Skybox", &showSkybox);
                    ImGui::Checkbox("Terrain", &showTerrain);
                    ImGui::Checkbox("Vegetation", &showVegetation);
                    ImGui::Checkbox("Model", &showModel);
                    ImGui::Checkbox("Wireframe", &wireframe);
                }

                // ---------- Vegetation ----------
                if (ImGui::CollapsingHeader("Vegetation"))
                {
                    // ImGui 内部引用外部变量（需要用指针或引用）
                    ImGui::SliderFloat("Wind Strength", &windStrength, 0.0f, 2.0f);
                    ImGui::SliderFloat("Wind Speed", &windSpeed, 0.1f, 5.0f);
                }

                // ---------- Controls ----------
                if (ImGui::CollapsingHeader("Controls"))
                {
                    ImGui::BulletText("WASD - Move Camera");
                    ImGui::BulletText("Right Mouse Drag - Rotate View");
                    ImGui::BulletText("Scroll Wheel - Zoom");
                    ImGui::BulletText("ESC - Exit");
                }

                ImGui::End();

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

                // ============================================================
                // Shadow Pass：从光源视角渲染场景到 Shadow Map
                // ============================================================
                // 找到场景中的点光源作为阴影投射光源
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

                // 将光源的 View*Projection 矩阵写入 UBO（着色器用它计算阴影坐标）
                ubo.lightViewProj = shadowSystem.getLightViewProj();

                // ===== 写入风力参数 =====
                ubo.windTime = totalTime;
                ubo.windStrength = windStrength;
                ubo.windSpeed = windSpeed;
                // 风向：可以从风向来推导方向向量
                // 简单起见，用固定的方向（例如沿X轴）
                ubo.windDirectionX = 1.0f;
                ubo.windDirectionZ = 0.0f;

                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();

                // ============================================================
                // 主渲染通道：使用 Shadow Map 渲染最终场景
                // ============================================================
                lveRenderer.beginSwapChainRenderPass(commandBuffer);
                skyboxSystem.render(frameInfo, skyboxTexture, *skyboxModel);
                simpleRenderSystem->rendererGameObjects(frameInfo);
                vegetationSystem.render(frameInfo);
                pointLightSystem->renderer(frameInfo);
                lveRenderer.endSwapChainRenderPass(commandBuffer);

                imgui.render(commandBuffer);
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
        auto diffuseTex = std::make_shared<LveTexture>(lveDevice, "apple/image/Model_0.jpg");
        catMaterial->setTexture(LveMaterial::TextureType::DIFFUSE, diffuseTex);

        catMaterial->setBaseColorFactor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        catMaterial->setMetallicFactor(0.2f);
        catMaterial->setRoughnessFactor(0.5f);

        catMaterial->buildDescriptorSet();
        auto gameObj = LveGameObject::createGameObject();
        gameObj.model = LveModel::createModelFromFile(lveDevice, "apple/Model.obj");
        gameObj.material = catMaterial; // 直接赋值 material 成员
        gameObj.transform.translation = {0.f, 0.f, 0.f};
        gameObj.transform.rotation = {0.f, 0.f, 1.f * 3.14f};
        gameObj.transform.scale = glm::vec3{0.02f};

        gameObjects.emplace(gameObj.getId(), std::move(gameObj));

        auto terrainModel = lve::createTerrainModel(lveDevice,
                                                    "tt.jpg", // 高度图路径
                                                    200.0f,   // 宽度 200
                                                    200.0f,   // 深度 200
                                                    256,      // 分段数 256 (顶点 257x257)
                                                    -5.0f,    // 最低高度
                                                    20.0f,    // 最高高度
                                                    "tt.jpg");

        auto terrainMaterial = materialManager.createMaterial("terrain");
        auto terrainTex = std::make_shared<LveTexture>(lveDevice, "tt.jpg");
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
