#version 450

layout(location = 0) in vec3 inPosition;      // 模型局部坐标（四边形）
layout(location = 1) in vec3 inColor;         // 未使用，但需占位
layout(location = 2) in vec3 inNormal;        // 未使用
layout(location = 3) in vec2 inUV;            // 纹理坐标
layout(location = 4) in vec3 inInstancePos;   // 实例位置（世界坐标）
layout(location = 5) in float inInstanceScale;// 实例缩放

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec3 fragPosWorld;   // 新增：世界坐标输出
layout(location = 2) out vec3 fragNormalWorld;// 新增：法线输出

struct PointLight {
    vec4 position;
    vec4 color;
};
layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    vec4 ambientColor;
    int numLights;
    PointLight pointLights[10];
} ubo;

void main() {
    fragUV = inUV;

    vec3 cameraRight = ubo.inverseView[0].xyz;
    vec3 cameraUp    = ubo.inverseView[1].xyz;
    cameraUp = vec3(0.0, 1.0, 0.0);  // 保持直立

    // 广告牌偏移计算：quad底部(y=0)显示树根→在地面，quad顶部(y=1)显示树叶→在上方
    vec3 offset = cameraRight * inPosition.x * inInstanceScale
                + cameraUp    * inPosition.y * inInstanceScale;

    vec3 worldPos = inInstancePos + offset;
    gl_Position = ubo.projection * ubo.view * vec4(worldPos, 1.0);
    fragPosWorld = worldPos;                       // 传递世界坐标
    fragNormalWorld = vec3(0.0, 1.0, 0.0);        // 固定法线向上
}
