#include "lve_imgui.hpp"
#include "lve_descriptors.hpp"

#include <iostream>
#include <stdexcept>

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
}
