// Vulkan学习项目 — 键盘移动控制器实现
// 处理WASD移动、QE旋转和方向键视角旋转的键盘输入

#include "keyboard_movement_controller.hpp"
#include <limits>
namespace lve
{
    // 在XZ平面上移动和旋转游戏对象
    // 执行步骤：
    //   1. 检测方向键（lookLeft/Right/Up/Down）以更新视角旋转量
    //   2. 检测QE键（rotateLeft/Right）以更新自身旋转
    //   3. 归一化总旋转量并按lookSpeed*dt缩放，更新rotation
    //   4. 对pitch角度做[-1.5, 1.5]钳制防止过转
    //   5. 对yaw角度取模保持在[0, 2π]范围内
    //   6. 根据当前yaw计算前方向和右方向
    //   7. 检测WASD和JK键计算移动方向
    //   8. 归一化移动方向并按moveSpeed*dt缩放应用到translation
    void KeyBoardMovementController::moveInPlaneXZ(GLFWwindow *window, float dt, LveGameObject &gameobject)
    {
        // 方向键控制视角旋转
        glm::vec3 rotation{0};
        if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS)
        {
            rotation.y += 1.f;
        }
        if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS)
        {
            rotation.y -= 1.f;
        }
        if (glfwGetKey(window, keys.lookUp) == GLFW_PRESS)
        {
            rotation.x -= 1.f;
        }
        if (glfwGetKey(window, keys.lookDown) == GLFW_PRESS)
        {
            rotation.x += 1.f;
        }

        // Q键向左旋转，E键向右旋转
        if (glfwGetKey(window, keys.rotateLeft) == GLFW_PRESS)
        {
            gameobject.transform.rotation.z -= rotateSpeed * dt;
        }
        if (glfwGetKey(window, keys.rotateRight) == GLFW_PRESS)
        {
            gameobject.transform.rotation.z += rotateSpeed * dt;
        }
        // 应用归一化的视角旋转
        if (glm::dot(rotation, rotation) > std::numeric_limits<float>::epsilon())
        {
            gameobject.transform.rotation += lookSpeed * dt * glm::normalize(rotation);
        }

        // 限制俯仰角范围，防止摄像机翻转
        gameobject.transform.rotation.x = glm::clamp(gameobject.transform.rotation.x, -1.5f, 1.5f);
        gameobject.transform.rotation.y = glm::mod(gameobject.transform.rotation.y, glm::two_pi<float>());

        // 根据偏航角计算前、右、上方向
        float yaw = gameobject.transform.rotation.y;
        const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
        const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
        const glm::vec3 upDir{0.f, 1.f, 0.f};

        // WASD移动（水平）和JK升降（垂直）
        glm::vec3 moveDir{0.f};
        if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS)
        {
            moveDir += forwardDir;
        }
        if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS)
        {
            moveDir -= forwardDir;
        }
        if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS)
        {
            moveDir += rightDir;
        }
        if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS)
        {
            moveDir -= rightDir;
        }
        if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS)
        {
            moveDir.y -= 1.0f;
        }
        if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS)
        {
            moveDir.y += 1.0f;
        }

        // 应用归一化的移动量
        if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon())
        {
            gameobject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
        }
    }
}
