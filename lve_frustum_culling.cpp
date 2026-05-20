#include "lve_frustum_culling.hpp"
#include <cmath>
#include <cstring>

namespace lve {

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  从 View × Projection 矩阵提取6个裁剪平面                               ║
// ║                                                                         ║
// ║  【为什么这么做？】                                                      ║
// ║                                                                         ║
// ║  一个4×4的矩阵 M 有4行: M[0], M[1], M[2], M[3] (每行是一个 vec4)         ║
// ║                                                                         ║
// ║  在齐次裁剪空间中，一个点 P 被变换后：P' = M * P                         ║
// ║  判断 P' 是否在视锥体内需要检查:                                          ║
// ║    -w' <= x' <= w'     (左右)                                           ║
// ║    -w' <= y' <= w'     (上下)                                           ║
// ║     0' <= z' <= w'     (Vulkan 的深度范围是 0到1)                        ║
// ║                                                                         ║
// ║  以"左平面"为例:  x' >= -w'  →  x' + w' >= 0                            ║
// ║    展开: (M[0]·P) + (M[3]·P) >= 0                                       ║
// ║    合并: (M[0] + M[3]) · P >= 0                                         ║
// ║    这正是平面方程: A*x + B*y + C*z + D >= 0                              ║
// ║    其中 [A,B,C,D] = M[0] + M[3]                                         ║
// ║                                                                         ║
// ║  【6个平面怎么来的？】                                                    ║
// ║    左平面:  M[3] + M[0]    (从 x + w >= 0)                               ║
// ║    右平面:  M[3] - M[0]    (从 -x + w >= 0 → x - w <= 0)                ║
// ║    下平面:  M[3] + M[1]    (从 y + w >= 0)                               ║
// ║    上平面:  M[3] - M[1]    (从 -y + w >= 0 → y - w <= 0)                ║
// ║    近平面:  M[3] + M[2]    (Vulkan: 从 z >= 0)                          ║
// ║    远平面:  M[3] - M[2]    (Vulkan: 从 z - w <= 0 → z <= w)            ║
// ║                                                                         ║
// ║  【为什么加/减？】                                                        ║
// ║    Gribb/Hartmann 的经典论文推导: 直接用矩阵行做线性组合就能得到          ║
// ║    世界空间中的平面方程，不需要先变换到裁剪空间再判断。                    ║
// ║    这比变换每个顶点再判断高效得多！                                       ║
// ╚══════════════════════════════════════════════════════════════════════════╝
void Frustum::extractFromMatrix(const glm::mat4& viewProjMatrix) {
    // glm::mat4 在内存中是**列主序**存储的
    // 但 glm 重载了 operator[]，让 mat4[i] 返回第 i **列** (vec4)
    // 所以我们用 mat4[i] 拿到第 i 列
    // 要得到第 i 行，需要用 mat4[col][row] 的形式

    // 提取行向量（从列主序矩阵中手动取出）
    // rowX.y 意思是: 第 X 行的第 col=1 个分量，即列向量 [1] 的第 X 个分量
    // 这样理解: 矩阵乘法中，矩阵的第 i 行 点乘 点的向量
    //
    // 可视化: 矩阵 M 的4列是 col0, col1, col2, col3:
    //   col0 = (m00, m01, m02, m03) = (M[0][0], M[0][1], M[0][2], M[0][3])
    //   但 glm 中 M[0] 返回的是列0，所以:
    //     row0 = (M[0][0], M[1][0], M[2][0], M[3][0])
    //     row1 = (M[0][1], M[1][1], M[2][1], M[3][1])
    //     row2 = (M[0][2], M[1][2], M[2][2], M[3][2])
    //     row3 = (M[0][3], M[1][3], M[2][3], M[3][3])

    glm::vec4 row0 = glm::vec4(viewProjMatrix[0][0], viewProjMatrix[1][0],
                               viewProjMatrix[2][0], viewProjMatrix[3][0]);
    glm::vec4 row1 = glm::vec4(viewProjMatrix[0][1], viewProjMatrix[1][1],
                               viewProjMatrix[2][1], viewProjMatrix[3][1]);
    glm::vec4 row2 = glm::vec4(viewProjMatrix[0][2], viewProjMatrix[1][2],
                               viewProjMatrix[2][2], viewProjMatrix[3][2]);
    glm::vec4 row3 = glm::vec4(viewProjMatrix[0][3], viewProjMatrix[1][3],
                               viewProjMatrix[2][3], viewProjMatrix[3][3]);

    // ---- 用行的线性组合生成6个平面 ----
    // 每个平面 = (A, B, C, D)
    // 其中 (A, B, C) 是法向量分量，D 是距离常数项

    // 左平面: row3 + row0
    m_planes[LEFT]   = {glm::vec3(row3 + row0), row3.w + row0.w};

    // 右平面: row3 - row0
    m_planes[RIGHT]  = {glm::vec3(row3 - row0), row3.w - row0.w};

    // 下平面: row3 + row1
    m_planes[BOTTOM] = {glm::vec3(row3 + row1), row3.w + row1.w};

    // 上平面: row3 - row1
    m_planes[TOP]    = {glm::vec3(row3 - row1), row3.w - row1.w};

    // 近平面: row2  (Vulkan 深度范围 [0,1]，近平面 z=0 → row2·P >= 0)
    m_planes[NEAR]   = {glm::vec3(row2), row2.w};

    // 远平面: row3 - row2  (Vulkan 远平面 z=w → (row3-row2)·P >= 0)
    m_planes[FAR]    = {glm::vec3(row3 - row2), row3.w - row2.w};

    // ---- 归一化所有平面 ----
    // 为什么要归一化？因为直接做行组合得到的法向量长度不是1
    // 不归一化的话，signedDistanceToPoint() 返回的不是真实的距离
    // 但剔除判断只需要符号，理论上可以不归一化
    // 不过归一化后距离值有意义，方便调试
    for (auto& plane : m_planes) {
        float length = glm::length(plane.normal);
        if (length > 0.000001f) { // 防止除以零
            plane.normal   /= length;
            plane.distance /= length;
        }
    }
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  判断 AABB 是否在视锥体内                                                ║
// ║                                                                         ║
// ║  【算法思路】                                                            ║
// ║                                                                         ║
// ║  对每个平面，我们需要判断 AABB 是在平面的内侧还是外侧。                     ║
// ║                                                                         ║
// ║  朴素做法: 测试 AABB 的8个顶点                                             ║
// ║  高效做法: 只测试 **离平面最近的顶点**                                    ║
// ║                                                                         ║
// ║  【哪个顶点离平面最近？】                                                  ║
// ║                                                                         ║
// ║  对于法向量 n = (nx, ny, nz):                                            ║
// ║    - 如果 nx > 0: 离平面最近的点取 min.x (向左靠)                         ║
// ║    - 如果 nx < 0: 离平面最近的点取 max.x (向右靠)                         ║
// ║                                                                         ║
// ║  即: 离平面最近的点 = (nx>0 ? min.x : max.x, 同理y, 同理z)               ║
// ║                                                                         ║
// ║  为什么？法向量指向视锥体内部，如果法向量 x 分量为正，说明"里面"是 +x 方向， ║
// ║  所以 -x 方向(min.x) 离"外面"最近，也离平面最近。                          ║
// ║                                                                         ║
// ║  【关键判断】                                                              ║
// ║                                                                         ║
// ║  如果**离平面最近的顶点**都在平面外侧 (signedDistance < 0)，                ║
// ║  那么整个 AABB 的 8 个顶点都在平面外侧 → AABB 完全在视锥体外 → 剔除！       ║
// ║                                                                         ║
// ║  如果对所有6个平面，"最近点"都在内侧 → AABB 完全或部分在视锥体内 → 可见。   ║
// ╚══════════════════════════════════════════════════════════════════════════╝
bool Frustum::isAABBVisible(const AABB& aabb) const {
    // 遍历6个平面
    for (const auto& plane : m_planes) {
        // 第1步: 找到离这个平面**最远**的顶点（n-vertex）
        // n-vertex 是 AABB 的8个顶点中 signed distance 最大的那个
        // 也就是沿法向量方向最靠里的顶点
        // 如果连这个"最靠里"的顶点都在平面外侧 → 整个AABB就在外侧了
        glm::vec3 nVertex;
        nVertex.x = (plane.normal.x > 0.0f) ? aabb.max.x : aabb.min.x;
        nVertex.y = (plane.normal.y > 0.0f) ? aabb.max.y : aabb.min.y;
        nVertex.z = (plane.normal.z > 0.0f) ? aabb.max.z : aabb.min.z;

        // 第2步: 用 n-vertex 测试
        // 如果 n-vertex 都在平面外侧 → 说明8个顶点全在外侧 → 整个AABB被剔除
        if (plane.signedDistanceToPoint(nVertex) < 0.0f) {
            return false; // 被这个平面完全切掉了
        }
    }
    return true; // 通过了所有6个平面的测试 → 可见
}

} // namespace lve
