#version 450
// 粒子顶点着色器：广告牌技术渲染
// 每个粒子渲染为一个始终面向摄像机的四边形
// 顶点数据从SSBO（作为实例化顶点缓冲）读取

// 四边形局部坐标（binding 0）
layout(location = 0) in vec3 inPosition;    // 局部空间位置 [-0.5, 0.5]
layout(location = 1) in vec3 inColor;       // 四边形颜色（未使用）
layout(location = 2) in vec3 inNormal;      // 法线（未使用）
layout(location = 3) in vec2 inUV;          // 纹理坐标

// 粒子实例数据（binding 1，来自SSBO）
layout(location = 4) in vec3 inInstancePos;    // 粒子世界位置
layout(location = 5) in vec3 inInstanceColor;  // 粒子颜色
layout(location = 6) in float inInstanceSize;  // 粒子大小

// 输出到片元着色器
layout(location = 0) out vec2 fragUV;         // 纹理坐标
layout(location = 1) out vec3 fragColor;      // 粒子颜色

// 点光源数据结构（与CPU端保持一致）
struct PointLight
{
    vec4 position;
    vec4 color;
};

// set=0: 全局UBO（投影/视图矩阵、光源等）
layout(set = 0, binding = 0) uniform GlobalUbo
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseView;
    mat4 lightViewProj;
    vec4 ambientColor;
    int numLights;
    PointLight pointLights[10];
} ubo;

void main()
{
    // 传递纹理坐标和颜色到片元着色器
    fragUV = inUV;
    fragColor = inInstanceColor;

    // ===== 广告牌技术：使四边形始终面向摄像机 =====
    // 从逆视图矩阵提取摄像机的右向量和上向量
    vec3 cameraRight = vec3(ubo.viewMatrix[0][0], ubo.viewMatrix[1][0], ubo.viewMatrix[2][0]);
    vec3 cameraUp    = vec3(ubo.viewMatrix[0][1], ubo.viewMatrix[1][1], ubo.viewMatrix[2][1]);

    // 粒子世界位置
    vec3 worldPos = inInstancePos;

    // 广告牌偏移：四边形局部坐标映射到世界空间
    // inPosition.x 沿摄像机右方向偏移
    // inPosition.y 沿摄像机上方向偏移
    vec3 billboardOffset = cameraRight * inPosition.x * inInstanceSize
                         + cameraUp    * inPosition.y * inInstanceSize;

    // 最终世界空间位置
    vec3 finalWorldPos = worldPos + billboardOffset;

    // 变换到裁剪空间
    gl_Position = ubo.projectionMatrix * ubo.viewMatrix * vec4(finalWorldPos, 1.0);
}