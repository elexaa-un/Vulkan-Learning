// Vulkan学习项目 — 摄像机类实现
// 正交投影、透视投影和多种视图矩阵设置方式的具体实现

#include "lve_camera.hpp"
#include "lve_utils.hpp"

#include <limits>
namespace lve
{
    // 设置正交投影矩阵
    void LveCamera::setOrthographicProjection(
        float top, float bottom, float left, float right, float near, float far)
    {
        projectionMatrix = glm::mat4{1.0f};
        projectionMatrix[0][0] = 2.f / (right - left);
        projectionMatrix[1][1] = 2.f / (bottom - top);
        projectionMatrix[2][2] = 1.f / (far - near);
        projectionMatrix[3][0] = -(right + left) / (right - left);
        projectionMatrix[3][1] = -(bottom + top) / (bottom - top);
        projectionMatrix[3][2] = -near / (far - near);
    }

    // 设置透视投影矩阵
    // 使用反向Y轴（Vulkan NDC的Y轴朝下，乘以-1修正）
    void LveCamera::setPerspectiveProject(float fovy, float aspect, float near, float far)
    {
        LVE_ASSERT(glm::abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f, "aspect ratio must be non-zero");
        const float tanHalfFovy = tan(fovy / 2.f);
        projectionMatrix = glm::mat4{1.0f};
        projectionMatrix[0][0] = 1.f / (aspect * tanHalfFovy);
        projectionMatrix[1][1] = 1.f / tanHalfFovy;
        projectionMatrix[2][2] = far / (far - near);
        projectionMatrix[2][3] = 1.f;
        projectionMatrix[3][2] = -(far * near) / (far - near);

        // 反转Y轴以适配Vulkan的NDC（Y轴向下）
        projectionMatrix[1][1] *= -1.0f;
    }

    // 通过方向向量设置视图矩阵
    // 执行步骤：
    //   1. 归一化方向向量得到前向量 (w)
    //   2. 计算右向量 (u) = normalize(w × up)
    //   3. 计算上向量 (v) = w × u
    //   4. 构建正交旋转矩阵作为视图矩阵
    //   5. 计算平移部分：-dot(u, position), -dot(v, position), -dot(w, position)
    //   6. 构建逆视图矩阵（旋转矩阵的转置 + 位置作为第四列）
    void LveCamera::setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up)
    {
        const glm::vec3 w{glm::normalize(direction)};
        const glm::vec3 u{glm::normalize(glm::cross(w, up))};
        const glm::vec3 v{glm::cross(w, u)};

        viewMatrix = glm::mat4{1.f};
        viewMatrix[0][0] = u.x;
        viewMatrix[1][0] = u.y;
        viewMatrix[2][0] = u.z;
        viewMatrix[0][1] = v.x;
        viewMatrix[1][1] = v.y;
        viewMatrix[2][1] = v.z;
        viewMatrix[0][2] = w.x;
        viewMatrix[1][2] = w.y;
        viewMatrix[2][2] = w.z;
        viewMatrix[3][0] = -glm::dot(u, position);
        viewMatrix[3][1] = -glm::dot(v, position);
        viewMatrix[3][2] = -glm::dot(w, position);

        inverseViewMatrix = glm::mat4{1.f};
        inverseViewMatrix[0][0] = u.x;
        inverseViewMatrix[0][1] = u.y;
        inverseViewMatrix[0][2] = u.z;
        inverseViewMatrix[1][0] = v.x;
        inverseViewMatrix[1][1] = v.y;
        inverseViewMatrix[1][2] = v.z;
        inverseViewMatrix[2][0] = w.x;
        inverseViewMatrix[2][1] = w.y;
        inverseViewMatrix[2][2] = w.z;
        inverseViewMatrix[3][0] = position.x;
        inverseViewMatrix[3][1] = position.y;
        inverseViewMatrix[3][2] = position.z;
    }

    // 通过观察目标设置视图矩阵（计算方向并委托给setViewDirection）
    void LveCamera::setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up)
    {
        setViewDirection(position, target - position, up);
    }

    // 通过YXZ欧拉角设置视图矩阵
    // 旋转顺序：绕Y轴旋转(偏航) → 绕X轴旋转(俯仰) → 绕Z轴旋转(滚转)
    void LveCamera::setViewYXZ(glm::vec3 position, glm::vec3 rotation)
    {
        const float c3 = glm::cos(rotation.z);
        const float s3 = glm::sin(rotation.z);
        const float c2 = glm::cos(rotation.x);
        const float s2 = glm::sin(rotation.x);
        const float c1 = glm::cos(rotation.y);
        const float s1 = glm::sin(rotation.y);
        const glm::vec3 u{(c1 * c3 + s1 * s2 * s3), (c2 * s3), (c1 * s2 * s3 - c3 * s1)};
        const glm::vec3 v{(c3 * s1 * s2 - c1 * s3), (c2 * c3), (c1 * c3 * s2 + s1 * s3)};
        const glm::vec3 w{(c2 * s1), (-s2), (c1 * c2)};
        viewMatrix = glm::mat4{1.f};
        viewMatrix[0][0] = u.x;
        viewMatrix[1][0] = u.y;
        viewMatrix[2][0] = u.z;
        viewMatrix[0][1] = v.x;
        viewMatrix[1][1] = v.y;
        viewMatrix[2][1] = v.z;
        viewMatrix[0][2] = w.x;
        viewMatrix[1][2] = w.y;
        viewMatrix[2][2] = w.z;
        viewMatrix[3][0] = -glm::dot(u, position);
        viewMatrix[3][1] = -glm::dot(v, position);
        viewMatrix[3][2] = -glm::dot(w, position);

        inverseViewMatrix = glm::mat4{1.f};
        inverseViewMatrix[0][0] = u.x;
        inverseViewMatrix[0][1] = u.y;
        inverseViewMatrix[0][2] = u.z;
        inverseViewMatrix[1][0] = v.x;
        inverseViewMatrix[1][1] = v.y;
        inverseViewMatrix[1][2] = v.z;
        inverseViewMatrix[2][0] = w.x;
        inverseViewMatrix[2][1] = w.y;
        inverseViewMatrix[2][2] = w.z;
        inverseViewMatrix[3][0] = position.x;
        inverseViewMatrix[3][1] = position.y;
        inverseViewMatrix[3][2] = position.z;
    }
}
