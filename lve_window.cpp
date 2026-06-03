// Vulkan学习项目 — 窗口实现
// GLFW窗口的创建、销毁和Vulkan表面创建

#include "lve_window.hpp"
#include <stdexcept>
namespace lve
{
    // 构造函数：初始化GLFW并创建窗口
    LveWindow::LveWindow(int w, int h, std::string name) : width{w}, height{h}, windowName{name}
    {
        initWindow();
    }

    // 析构函数：销毁GLFW窗口并终止GLFW
    LveWindow::~LveWindow()
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    // 初始化GLFW窗口
    // 执行步骤：
    //   1. 调用 glfwInit() 初始化GLFW库
    //   2. 设置 GLFW_CLIENT_API=GLFW_NO_API（禁用OpenGL上下文）
    //   3. 设定窗口可调整大小
    //   4. 创建指定尺寸和标题的窗口
    //   5. 通过 glfwSetWindowUserPointer 绑定 this 指针
    //   6. 绑定帧缓冲大小变化回调
    void LveWindow::initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    // 通过GLFW创建Vulkan窗口表面
    void LveWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surf)
    {
        if (glfwCreateWindowSurface(instance, window, nullptr, surf) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface");
        }
    }

    // 帧缓冲大小变化回调：更新窗口尺寸并标记 framebufferResized 标志
    void LveWindow::framebufferResizeCallback(GLFWwindow *window, int width, int height)
    {
        auto lveWindow = reinterpret_cast<LveWindow *>(glfwGetWindowUserPointer(window));
        lveWindow->framebufferResized = true;
        lveWindow->width = width;
        lveWindow->height = height;
    }
}
