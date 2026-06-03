// Vulkan学习项目 — 窗口管理类
// 通过GLFW创建和管理窗口，提供Vulkan表面创建和窗口事件处理

#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <string>
namespace lve
{
    // GLFW窗口封装：管理窗口生命周期和Vulkan表面创建
    class LveWindow
    {
    public:
        // 构造函数：初始化GLFW窗口
        // 执行步骤：
        //   1. 调用 glfwInit() 初始化GLFW库
        //   2. 设置 Vulkan 相关窗口提示（无OpenGL上下文）
        //   3. 创建指定宽高和标题的窗口
        //   4. 绑定 framebufferResizeCallback 回调
        LveWindow(int w, int h, std::string name);
        ~LveWindow();

        // 禁止拷贝（GLFW窗口资源唯一）
        LveWindow(const LveWindow &) = delete;
        LveWindow &operator=(const LveWindow &) = delete;

        // 检查窗口是否应关闭（收到GLFW关闭事件）
        bool shouldClose() { return glfwWindowShouldClose(window); }

        // 获取当前窗口尺寸（以VkExtent2D格式）
        VkExtent2D getExtent() { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }

        // 查询/重置帧缓冲大小变化标志
        bool wasWindowResized() { return framebufferResized; }
        void resetWindowResizedFlag() { framebufferResized = false; }

        // 为Vulkan实例创建窗口表面（通过glfwCreateWindowSurface）
        void createWindowSurface(VkInstance instance, VkSurfaceKHR *surf);

        // 获取原生GLFW窗口句柄
        GLFWwindow *getGLFWwindow() const { return window; }

    private:
        // 静态回调：GLFW窗口大小变化时调用，设置 framebufferResized 标志
        static void framebufferResizeCallback(GLFWwindow *window, int width, int height);

        // 内部窗口初始化（设置GLFW提示和创建窗口）
        void initWindow();

        int width;                         // 窗口宽度（像素）
        int height;                        // 窗口高度（像素）
        bool framebufferResized = false;   // 帧缓冲大小变化标志

        std::string windowName;            // 窗口标题
        GLFWwindow *window;                // GLFW窗口句柄
    };
}
