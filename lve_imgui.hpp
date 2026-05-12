#pragma once
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_vulkan.h"
#include "ImGui/imgui_impl_win32.h"
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "lve_device.hpp"
#include "lve_descriptors.hpp" // 新增：需要 LveDescriptorPool

namespace lve
{
    class LveImgui
    {
    public:
        LveImgui(const LveImgui &) = delete;
        LveImgui &operator=(const LveImgui &) = delete;

        // 构造函数：只需要窗口、设备和渲染通道
        LveImgui(GLFWwindow *window, LveDevice &device, VkRenderPass renderPass);
        ~LveImgui();

        // === 每帧调用的方法 ===

        // 在每帧开始时调用，启动新的一帧
        // 调用后，你就可以写自己的 ImGui 控件代码了
        void newFrame();

        // 在每帧结束时调用，把 UI 渲染到屏幕上
        // 必须在 render pass 结束之后调用
        void render(VkCommandBuffer commandBuffer);

    private:
        LveDevice &device;
        // 【关键】把 descriptor pool 保存为成员变量，保证生命周期
        std::unique_ptr<LveDescriptorPool> imguiPool;
    };
}
