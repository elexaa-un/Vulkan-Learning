// Vulkan学习项目 — 视锥体剔除系统
// 从View×Projection矩阵提取六个裁剪平面，对AABB包围盒进行可见性检测

#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <algorithm>

namespace lve {

// ╔══════════════════════════════════════════════════════════════════╗
// ║  视锥体的一个裁剪平面                                            ║
// ║  平面方程: normal.x * Px + normal.y * Py + normal.z * Pz + d = 0 ║
// ║  对于视锥体内侧的点: normal · P + d >= 0                         ║
// ╚══════════════════════════════════════════════════════════════════╝
struct FrustumPlane {
    glm::vec3 normal;   // 指向视锥体内部的法向量 (归一化后)
    float     distance; // 平面方程中的 d 值（不是到原点的距离，是方程常数项）

    /// 计算任意一点到平面的**有符号距离**
    /// 返回值 >= 0 → 点在视锥体内侧
    /// 返回值  < 0 → 点在视锥体外侧
    float signedDistanceToPoint(const glm::vec3& point) const {
        return glm::dot(normal, point) + distance;
    }
};

// ╔══════════════════════════════════════════════════════════════════╗
// ║  AABB = Axis-Aligned Bounding Box (轴对齐包围盒)                  ║
// ║  用两个对角顶点就能完整描述一个长方体:                              ║
// ║    min = (x_min, y_min, z_min)  ← 最小的那个角                    ║
// ║    max = (x_max, y_max, z_max)  ← 最大的那个角                    ║
// ╚══════════════════════════════════════════════════════════════════╝
struct AABB {
    glm::vec3 min;  // 最小的那个角点
    glm::vec3 max;  // 最大的那个角点

    /// 创建一个空包围盒（用于迭代构建）
    static AABB empty() {
        return AABB{
            glm::vec3{std::numeric_limits<float>::max()},
            glm::vec3{-std::numeric_limits<float>::max()}
        };
    }

    /// 扩展包围盒以包含一个点
    void extend(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    /// 获取包围盒的中心点
    glm::vec3 center() const {
        return (min + max) * 0.5f;
    }

    /// 获取包围盒的半尺寸（从中心到面的距离）
    glm::vec3 halfSize() const {
        return (max - min) * 0.5f;
    }

    /// 获取8个顶点坐标（用于调试或变换）
    std::array<glm::vec3, 8> corners() const {
        return {{
            {min.x, min.y, min.z}, // 0: 左-下-近
            {max.x, min.y, min.z}, // 1: 右-下-近
            {min.x, max.y, min.z}, // 2: 左-上-近
            {max.x, max.y, min.z}, // 3: 右-上-近
            {min.x, min.y, max.z}, // 4: 左-下-远
            {max.x, min.y, max.z}, // 5: 右-下-远
            {min.x, max.y, max.z}, // 6: 左-上-远
            {max.x, max.y, max.z}  // 7: 右-上-远
        }};
    }

    /// 将局部空间的 AABB 变换到世界空间
    /// 思路：把8个顶点都变换，然后找新的 min/max
    AABB transform(const glm::mat4& modelMatrix) const {
        auto corners = this->corners();
        AABB result = AABB::empty();
        for (const auto& corner : corners) {
            glm::vec4 worldCorner = modelMatrix * glm::vec4(corner, 1.0f);
            result.extend(glm::vec3(worldCorner));
        }
        return result;
    }
};

// ╔══════════════════════════════════════════════════════════════════╗
// ║  视锥体 = 6个平面的集合                                           ║
// ║  索引约定:                                                        ║
// ║    [0] = 左平面   (Left)                                         ║
// ║    [1] = 右平面   (Right)                                        ║
// ║    [2] = 下平面   (Bottom)                                       ║
// ║    [3] = 上平面   (Top)                                          ║
// ║    [4] = 近平面   (Near)                                         ║
// ║    [5] = 远平面   (Far)                                          ║
// ╚══════════════════════════════════════════════════════════════════╝
class Frustum {
public:
    enum PlaneIndex {
        LEFT   = 0,
        RIGHT  = 1,
        BOTTOM = 2,
        TOP    = 3,
        NEAR   = 4,
        FAR    = 5
    };

    Frustum() = default;

    /// 从 View × Projection 矩阵直接构造视锥体（最常用方式）
    explicit Frustum(const glm::mat4& viewProjMatrix) {
        extractFromMatrix(viewProjMatrix);
    }

    /// 从 View × Projection 矩阵提取6个裁剪平面
    /// 这是整个视锥体剔除的核心！
    void extractFromMatrix(const glm::mat4& viewProjMatrix);

    /// 判断一个 AABB 是否在视锥体内（或相交）
    /// @return true  = 可见（完全在内或部分相交）
    /// @return false = 完全在外 → 可以被剔除
    bool isAABBVisible(const AABB& aabb) const;

    /// 获取某个平面（用于调试）
    const FrustumPlane& getPlane(PlaneIndex index) const {
        return m_planes[static_cast<size_t>(index)];
    }

private:
    std::array<FrustumPlane, 6> m_planes;
};

} // namespace lve
