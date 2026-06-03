// Vulkan学习项目 — 摄像机类
// 管理投影矩阵和视图矩阵，支持透视/正交投影及多种视图设置方式

#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
namespace lve
{
    // 摄像机：封装了投影矩阵和视图矩阵的生成与查询
    class LveCamera
    {
    public:
        // 设置正交投影矩阵
        // 参数：top/bottom/left/right — 视锥体边界，near/far — 近/远裁剪面
        void setOrthographicProjection(
            float top, float bottom, float left, float right, float near, float far);

        // 设置透视投影矩阵
        // 参数：fovy — 垂直视场角（弧度），aspect — 宽高比，near/far — 近/远裁剪面
        void setPerspectiveProject(float fovy, float aspect, float near, float far);

        // 通过位置和方向设置视图矩阵
        // 执行步骤：
        //   1. 使用 position + direction 计算相机朝向
        //   2. 使用 glm::lookAt 生成 viewMatrix
        //   3. 计算 inverseViewMatrix
        void setViewDirection(
            glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, 1.f, 0.f});

        // 通过位置和目标点设置视图矩阵
        void setViewTarget(
            glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3{0.f, 1.f, 0.f});

        // 通过YXZ欧拉角设置视图矩阵（先Y轴旋转，再X轴，最后Z轴）
        void setViewYXZ(glm::vec3 position, glm::vec3 rotation);

        // 获取当前投影/视图/逆视图矩阵
        const glm::mat4 getProjection() const { return projectionMatrix; }
        const glm::mat4 getView() const { return viewMatrix; }
        const glm::mat4 getInverseView() const { return inverseViewMatrix; }

        // 从逆视图矩阵中提取相机世界空间位置
        const glm::vec3 getPosition() const { return glm::vec3(inverseViewMatrix[3]); }

    private:
        glm::mat4 projectionMatrix{1.f};     // 投影矩阵
        glm::mat4 viewMatrix{1.f};           // 视图矩阵
        glm::mat4 inverseViewMatrix{1.f};    // 逆视图矩阵（第四列即相机位置）
    };
}
