// Vulkan学习项目 — 键盘移动控制器
// 提供WASD移动、QE旋转和方向键视角旋转功能

#pragma once

#include "lve_gameobject.hpp"
#include "lve_window.hpp"

namespace lve
{
    // 键盘移动控制器：通过GLFW键盘输入控制游戏对象的移动和旋转
    class KeyBoardMovementController
    {
    public:
        // 按键映射配置：可自定义每种操作对应的键盘按键
        struct KeyMappings
        {
            int moveLeft = GLFW_KEY_A;          // 左移
            int moveRight = GLFW_KEY_D;         // 右移
            int moveForward = GLFW_KEY_W;       // 前进
            int moveBackward = GLFW_KEY_S;      // 后退
            int moveUp = GLFW_KEY_J;            // 上升
            int moveDown = GLFW_KEY_K;          // 下降
            int rotateLeft = GLFW_KEY_E;        // 左旋转（绕Y轴）
            int rotateRight = GLFW_KEY_Q;       // 右旋转（绕Y轴）
            int lookLeft = GLFW_KEY_LEFT;       // 视角左转
            int lookRight = GLFW_KEY_RIGHT;     // 视角右转
            int lookUp = GLFW_KEY_UP;           // 视角上转
            int lookDown = GLFW_KEY_DOWN;       // 视角下转
        };

        // 处理键盘输入，在XZ平面上移动和旋转游戏对象
        void moveInPlaneXZ(GLFWwindow *window, float dt, LveGameObject &gameobject);

        KeyMappings keys{};        // 按键映射配置
        float moveSpeed{3.f};      // 移动速度（单位/秒）
        float lookSpeed{5.f};      // 视角旋转速度（弧度/秒）
        float rotateSpeed{2.f};    // 自身旋转速度（弧度/秒）
    };
}
