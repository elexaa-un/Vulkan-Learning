// Vulkan学习项目 — ImGui调试界面实现
// 渲染参数控制面板、性能统计和场景切换UI

#include "lve_imgui.hpp"
#include "lve_descriptors.hpp"
#include "lve_frame_info.hpp"
#include "lve_weather_state.hpp"
#include <iostream>
#include <stdexcept>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
namespace lve
{
    LveImgui::LveImgui(GLFWwindow *window, LveDevice &device, VkRenderPass renderPass)
        : device(device)
    {
        // ===== 第一步：创建 ImGui 上下文 =====
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        // ===== 加载中文字体 =====
        // 默认位图字体只包含 ASCII/拉丁字符，中文需要 TTF 字体
        // 新版 ImGui (1.92+) 配合 Vulkan 后端支持动态字体图集，
        // 但仍需加载一个包含 CJK 字形的 TTF/OTF 字体作为字形来源。
        {
            ImGuiIO &io = ImGui::GetIO();

            // 关键：GetGlyphRangesChineseFull() 包含 3 万+ 汉字，
            // 默认 512x512 的字体图集纹理根本装不下，导致中文全部显示为 ???
            // 改用 GetGlyphRangesChineseSimplifiedCommon() 只加载 ~2500 个常用简体字，
            // 对调试 UI 来说完全够用，且能轻松塞进图集。
            // 同时增大图集纹理尺寸以防万一。

            // 对于 .ttc (TrueType Collection) 文件，需要配置 FontNo 指定字体面索引
            // 微软雅黑 msyh.ttc 的面 0 是常规体 (Microsoft YaHei)
            ImFontConfig fontConfig;
            fontConfig.FontNo = 0;        // TTC 中第一个字体面（常规体）
            fontConfig.MergeMode = false; // 非合并模式，重新开始
            fontConfig.PixelSnapH = true; // 像素对齐，字更清晰

            ImFont *font = io.Fonts->AddFontFromFileTTF(
                "C:\\Windows\\Fonts\\msyh.ttc",
                18.0f,
                &fontConfig,
                io.Fonts->GetGlyphRangesChineseSimplifiedCommon());

            if (font == nullptr)
            {
                std::cerr << "[ERROR] 无法加载中文字体 C:\\Windows\\Fonts\\msyh.ttc" << std::endl;
                std::cerr << "         中文将无法正常显示，请检查字体文件是否存在。" << std::endl;
            }
            else
            {
                std::cout << "[INFO] 中文字体加载成功: 微软雅黑 (Microsoft YaHei) 18px" << std::endl;
            }
        }

        // ===== 第二步：初始化 GLFW 后端 =====
        ImGui_ImplGlfw_InitForVulkan(window, true);

        // ===== 第三步：创建并配置 Descriptor Pool =====
        // 【重要】新版 ImGui（2026-04-22+）将 COMBINED_IMAGE_SAMPLER 拆成了两种独立类型
        //   - VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE：纹理图像视图
        //   - VK_DESCRIPTOR_TYPE_SAMPLER：采样器
        // 如果不提供这两种类型，ImGui 分配描述符集会直接失败崩溃！
        LveDescriptorPool::Builder poolBuilder{device};
        poolBuilder.addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, 1000);                // 新版 ImGui 需要
        poolBuilder.addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000);          // 新版 ImGui 需要
        poolBuilder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000); // 兼容旧版 ImGui
        poolBuilder.setPoolFlags(
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT); // ImGui 需要这个标志
        poolBuilder.setMaxSets(1000);
        imguiPool = poolBuilder.build(); // 保存为成员变量！

        // ===== 第四步：配置 Vulkan 初始化信息 =====
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = device.getInstance();
        init_info.PhysicalDevice = device.getPhysicalDevice();
        init_info.Device = device.device();
        init_info.QueueFamily = device.getGraphicsQueueFamilyIndex();
        init_info.Queue = device.graphicsQueue();
        init_info.DescriptorPool = imguiPool->getDescriptorPool();
        init_info.MinImageCount = 2;
        init_info.ImageCount = 3;
        init_info.PipelineInfoMain.RenderPass = renderPass;
        // init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        // ===== 第五步：初始化 ImGui Vulkan 后端 =====
        ImGui_ImplVulkan_Init(&init_info);

        // ===== 第六步：上传字体纹理 =====
        // 新版 ImGui（2026+）在 ImGui_ImplVulkan_Init() 时已自动完成字体上传，
        // 不再需要手动调用 CreateFontsTexture / DestroyFontUploadObjects。
        // ImGui_ImplVulkan_NewFrame() 会在首帧自动处理纹理更新。
        // 如果旧版需要手动上传，参考以下代码（当前版本已移除这些函数）：
        //   VkCommandBuffer cmd = device.beginSingleTimeCommands();
        //   ImGui_ImplVulkan_CreateFontsTexture(cmd);
        //   device.endSingleTimeCommands(cmd);
        //   vkDeviceWaitIdle(device.device());
        //   ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    LveImgui::~LveImgui()
    {
        vkDeviceWaitIdle(device.device());
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        // imguiPool 自动销毁（unique_ptr 的功劳）
    }

    void LveImgui::newFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void LveImgui::render(VkCommandBuffer commandBuffer)
    {
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    }

    // Weather type name lookup table
    static const char* kWeatherTypeNames[] = {
        "Clear",
        "Cloudy",
        "Rain",
        "Storm",
        "Snow"
    };

    void LveImgui::showImGUI(float frameTime, LveGameObject::Map &gameObjects, LveCamera &camera, LveGameObject &viewerObject, RenderOptions &options, FrameInfo* frameInfo, WeatherStateManager* weatherManager)
    {
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

            // ==== 视锥体剔除统计（仅在有 FrameInfo 时显示） ====
            if (frameInfo)
            {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "=== Frustum Culling ===");
                ImGui::Text("  Total candidates: %u", frameInfo->totalCount);
                ImGui::Text("  Culled (skipped):  %u", frameInfo->culledCount);
                ImGui::Text("  Rendered:          %u",
                            frameInfo->totalCount - frameInfo->culledCount);

                float cullRatio = 0.f;
                if (frameInfo->totalCount > 0)
                    cullRatio = (float)frameInfo->culledCount / (float)frameInfo->totalCount * 100.f;
                ImGui::Text("  Cull ratio:        %.1f%%", cullRatio);

                // 进度条可视化
                ImGui::ProgressBar(cullRatio / 100.f, ImVec2(-1.f, 0.f),
                                   frameInfo->culledCount > 0 ? "objects culled" : "no culling");
            }
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
            // 【修复】每次 ImGui 帧都从场景中读取当前状态，确保面板显示实时值
            // 先找到场景中的点光源
            for (auto &kv : gameObjects)
            {
                if (kv.second.pointLight)
                {
                    auto &lightObj = kv.second;
                    float lightPos[3] = {
                        lightObj.transform.translation.x,
                        lightObj.transform.translation.y,
                        lightObj.transform.translation.z};
                    float lightIntensity = lightObj.pointLight->lightIntensity;
                    float lightRadius = lightObj.pointLight->lightIntensity; // 共用同一个参数
                    float lightColor[3] = {
                        lightObj.color.x,
                        lightObj.color.y,
                        lightObj.color.z};

                    // 每个控件独立修改，不再嵌套 if
                    ImGui::SliderFloat3("Light Position", lightPos, -50.f, 50.f);
                    lightObj.transform.translation.x = lightPos[0];
                    lightObj.transform.translation.y = lightPos[1];
                    lightObj.transform.translation.z = lightPos[2];

                    ImGui::SliderFloat("Intensity", &lightIntensity, 1.f, 500.f);
                    lightObj.pointLight->lightIntensity = lightIntensity;

                    ImGui::SliderFloat("Radius", &lightRadius, 1.f, 50.f);
                    // 这里 radius 和 intensity 之前共用，lightRadius 修改后暂无独立字段存放
                    // 如果有独立 radius 字段请替换此处

                    ImGui::ColorEdit3("Color", lightColor);
                    lightObj.color.x = lightColor[0];
                    lightObj.color.y = lightColor[1];
                    lightObj.color.z = lightColor[2];

                    break; // 只处理第一个点光源
                }
            }
        }

        // ---------- Weather Control (Phase 1 新增) ----------
        if (weatherManager && ImGui::CollapsingHeader("Weather Control", ImGuiTreeNodeFlags_DefaultOpen))
        {
            WeatherPreset blended = weatherManager->getBlendedPreset();

            // Current weather state
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "=== Current Weather ===");
            const char* currentName = kWeatherTypeNames[static_cast<int>(weatherManager->getCurrentType())];
            ImGui::Text("Weather: %s", currentName);

            if (weatherManager->isTransitioning())
            {
                const char* targetName = kWeatherTypeNames[static_cast<int>(weatherManager->getTargetType())];
                ImGui::Text("Transition to: %s", targetName);
                ImGui::ProgressBar(weatherManager->getTransitionProgress(), ImVec2(-1.f, 0.f),
                                   "Transitioning...");
            }

            // Weather type selection
            ImGui::Separator();
            ImGui::Text("=== Switch Weather ===");
            static int selectedWeatherType = 0;
            ImGui::Combo("Target Weather", &selectedWeatherType, kWeatherTypeNames, 5);

            static float transitionDuration = 3.0f;
            ImGui::SliderFloat("Transition Duration (s)", &transitionDuration, 0.5f, 30.0f, "%.1f");

            if (ImGui::Button("Trigger Transition"))
            {
                WeatherType target = static_cast<WeatherType>(selectedWeatherType);
                weatherManager->transitionTo(target, transitionDuration);
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel Transition"))
            {
                weatherManager->cancelTransition();
            }

            // Real-time blended parameters
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "=== Blended Parameters ===");
            ImGui::Text("Cloud Coverage: %.2f", blended.cloudCoverage);
            ImGui::Text("Cloud Density: %.2f", blended.cloudDensity);
            ImGui::Text("Fog Density: %.3f", blended.fogDensity);
            ImGui::Text("Visibility: %.0f m", blended.visibility);
            ImGui::Text("Precipitation Rate: %.2f", blended.precipitationRate);
            ImGui::Text("Precipitation Type: %s", blended.isSnow ? "Snow" : "Rain");
            ImGui::Text("Droplet Size: %.2f", blended.dropletSize);
            ImGui::Text("Droplet Speed: %.2f", blended.dropletSpeed);
            ImGui::Text("Sun Intensity: %.2f", blended.sunIntensity);
            ImGui::Text("Ambient Intensity: %.2f", blended.ambientIntensity);
            ImGui::Text("Ambient Color: (%.2f, %.2f, %.2f)",
                        blended.ambientColor.r, blended.ambientColor.g, blended.ambientColor.b);
            ImGui::Text("Ground Wetness: %.2f", blended.groundWetness);
            ImGui::Text("Snow Accumulation: %.2f", blended.snowAccumulation);
            ImGui::Text("Lightning Frequency: %.2f/s", blended.lightningFrequency);

            // ---- Global Time Control ----
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "=== Global Time ===");
            ImGui::Text("Simulation Time: %.1f s", weatherManager->getSimulationTime());

            float tod = weatherManager->getTimeOfDay();
            ImGui::SliderFloat("Time of Day (0-24h)", &tod, 0.0f, 24.0f, "%.1f h");
            weatherManager->setTimeOfDay(tod);

            // Formatted clock display
            int hours = static_cast<int>(tod);
            int minutes = static_cast<int>((tod - hours) * 60.0f);
            ImGui::Text("Clock: %02d:%02d", hours % 24, minutes);

            float cycleSpeed = weatherManager->getDayNightCycleSpeed();
            ImGui::SliderFloat("Day/Night Cycle Speed (h/s)", &cycleSpeed, 0.0f, 0.5f, "%.3f");
            weatherManager->setDayNightCycleSpeed(cycleSpeed);

            float simScale = weatherManager->getSimulationTimeScale();
            ImGui::SliderFloat("Simulation Time Scale", &simScale, 0.0f, 10.0f, "%.1fx");
            weatherManager->setSimulationTimeScale(simScale);
        }

        // ---------- Render Options ----------
        if (ImGui::CollapsingHeader("Render Options"))
        {
            // 【修复】使用持久化的 options 引用，不再是 static 局部变量
            ImGui::Checkbox("Skybox", &options.showSkybox);
            ImGui::Checkbox("Terrain", &options.showTerrain);
            ImGui::Checkbox("Vegetation", &options.showVegetation);
            ImGui::Checkbox("Model", &options.showModel);
            ImGui::Checkbox("Wireframe", &options.wireframe);
        }

        // ---------- Vegetation ----------
        // if (ImGui::CollapsingHeader("Vegetation"))
        // {
        //     // ImGui 内部引用外部变量（需要用指针或引用）
        //     ImGui::SliderFloat("Wind Strength", &windStrength, 0.0f, 2.0f);
        //     ImGui::SliderFloat("Wind Speed", &windSpeed, 0.1f, 5.0f);
        // }

        // ---------- Controls ----------
        if (ImGui::CollapsingHeader("Controls"))
        {
            ImGui::BulletText("WASD - Move Camera");
            ImGui::BulletText("Right Mouse Drag - Rotate View");
            ImGui::BulletText("Scroll Wheel - Zoom");
            ImGui::BulletText("F1 - Toggle Debug Panel");
            ImGui::BulletText("ESC - Exit");
        }

        ImGui::End(); // 配对 ImGui::Begin
    }
}