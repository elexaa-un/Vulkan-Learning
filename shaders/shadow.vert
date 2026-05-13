#version 450

// ============================================================
// Shadow Map 顶点着色器
// ============================================================
// 作用：从光源视角渲染场景，只输出顶点位置到裁剪空间。
//       片段着色器不需要输出颜色，Vulkan 会自动写入深度。
//
// 数据流：
//   position (模型空间) 
//     → modelMatrix (世界空间) 
//     → lightViewProj (光源裁剪空间) 
//     → gl_Position (深度自动写入 Shadow Map)
// ============================================================

layout(location = 0) in vec3 position;

// 光源的 View-Proj 矩阵（通过 push constant 传入）
layout(push_constant) uniform ShadowPush {
    mat4 modelMatrix;      // 模型矩阵：把顶点从模型空间变换到世界空间
    mat4 lightViewProj;    // 光源的 View*Proj 矩阵：从世界空间变换到光源裁剪空间
} push;

void main() {
    // 1. 模型空间 → 世界空间 → 光源裁剪空间
    gl_Position = push.lightViewProj * push.modelMatrix * vec4(position, 1.0);
    
    // 不需要输出任何颜色或纹理坐标
    // Vulkan 会利用 gl_Position.z 自动填充深度附件
}
