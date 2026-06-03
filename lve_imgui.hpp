// Vulkan学习项目 — ImGui调试界面
// 集成Dear ImGui库，提供实时渲染参数调节、性能统计和场景控制面板

#pragma once
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_vulkan.h"
#include "ImGui/imgui_impl_win32.h"
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "lve_device.hpp"
#include "lve_descriptors.hpp" // 需要 LveDescriptorPool 来管理ImGui的描述符集
#include "lve_gameobject.hpp"
#include "lve_camera.hpp"
namespace lve
{
    // 渲染选项结构体：在主循环中持久化，供 ImGui 修改、渲染循环读取
    struct RenderOptions
    {
        bool showSkybox = true;       // 是否渲染天空盒
        bool showTerrain = true;      // 是否渲染地形
        bool showVegetation = true;   // 是否渲染植被
        bool showModel = true;        // 是否渲染普通模型
        bool wireframe = false;       // 是否以线框模式渲染
    };

    // ImGui集成类：封装初始化、帧更新和渲染提交
    class LveImgui
    {
    public:
        LveImgui(const LveImgui &) = delete;
        LveImgui &operator=(const LveImgui &) = delete;

        // 构造函数：初始化ImGui上下文，创建字体纹理，配置Vulkan渲染后端
        // 执行步骤：
        //   1. 创建ImGui上下文并设置样式
        //   2. 初始化ImGui_ImplGlfw 和 ImGui_ImplVulkan 后端
        //   3. 创建字体纹理并上传到GPU
        //   4. 创建专用的描述符池（imguiPool）
        LveImgui(GLFWwindow *window, LveDevice &device, VkRenderPass renderPass);
        ~LveImgui();

        // 在每帧开始时调用，启动新的一帧ImGui绘制
        void newFrame();

        // 显示完整的ImGui控制面板
        // 包含：性能信息、场景控制（天空盒/地形/植被开关）、光照参数、摄像机信息等
        void showImGUI(float frameTime,
                       LveGameObject::Map &gameObjects,
                       LveCamera &camera,
                       LveGameObject &viewerObject,
                       RenderOptions &options,
                       class FrameInfo* frameInfo = nullptr);

        // 在每帧结束时调用，将ImGui绘制命令提交到命令缓冲区
        // 必须在 render pass 结束之后调用
        void render(VkCommandBuffer commandBuffer);

    private:
        LveDevice &device;
        // ImGui专用的描述符池，保证生命周期与ImGui对象一致
        std::unique_ptr<LveDescriptorPool> imguiPool;
    };
}
