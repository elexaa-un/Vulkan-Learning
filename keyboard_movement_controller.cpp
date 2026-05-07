#include "keyboard_movement_controller.hpp"
#include <limits>
namespace lve
{
    void KeyBoardMovementController::moveInPlaneXZ(GLFWwindow *window, float dt, LveGameObject &gameobject)
    {
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
            rotation.x += 1.f;
        }
        if (glfwGetKey(window, keys.lookDown) == GLFW_PRESS)
        {
            rotation.x -= 1.f;
        }

        if (glfwGetKey(window, keys.rotateLeft) == GLFW_PRESS) // Q 键向左旋转
        {
            gameobject.transform.rotation.z -= rotateSpeed * dt;
        }
        if (glfwGetKey(window, keys.rotateRight) == GLFW_PRESS) // E 键向右旋转
        {
            gameobject.transform.rotation.z += rotateSpeed * dt;
        }
        if (glm::dot(rotation, rotation) > std::numeric_limits<float>::epsilon())
        {
            gameobject.transform.rotation += lookSpeed * dt * glm::normalize(rotation);
        }

        gameobject.transform.rotation.x = glm::clamp(gameobject.transform.rotation.x, -1.5f, 1.5f);
        gameobject.transform.rotation.y = glm::mod(gameobject.transform.rotation.y, glm::two_pi<float>());

        float yaw = gameobject.transform.rotation.y;
        const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
        const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
        const glm::vec3 upDir{0.f, 1.f, 0.f};

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
            moveDir.y += 1.0f;
        }
        if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS)
        {
            moveDir.y -= 1.0f;
        }

        if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon())
        {
            gameobject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
        }
    }
}