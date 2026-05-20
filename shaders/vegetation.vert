#version 450

layout(location = 0) in vec3 inPosition;       // quad局部坐标
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec3 inInstancePos;    // 实例世界位置
layout(location = 5) in float inInstanceScale; // 实例缩放

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec3 fragNormalWorld;

struct PointLight {
    vec4 position;
    vec4 color;
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseView;
    mat4 lightViewProj;      // ===== Shadow Map: 光源的 View*Proj 矩阵 =====
    vec4 ambientColor;
    int numLights;
    PointLight pointLights[10];
} ubo;

layout(push_constant) uniform Push {
        float windTime;       // 累计时间（秒）
        float windStrength;   // 风力强度 [0.0, 2.0]
        float windSpeed;      // 风速 [0.1, 5.0]
        float windDirectionX; // 风向 X 分量（归一化）
        float windDirectionZ; // 风向 Z 分量（归一化）
} push;
void main() {
    fragUV = inUV;

    vec3 cameraRight = ubo.inverseView[0].xyz;
    vec3 cameraUp    = ubo.inverseView[1].xyz;
    cameraUp = vec3(0.0, 1.0, 0.0);  // 保持直立

    // ===== 广告牌基础偏移 =====
    vec3 offset = cameraRight * inPosition.x * inInstanceScale
                + cameraUp    * inPosition.y * inInstanceScale;

    // ===== 新增：风力弯曲偏移 =====
    // Quad映射: Y=0=地面(树根), Y=1=顶部(树冠)
    // 直接用inPosition.y计算，树根不动，树冠摆动
    float worldHeight = inPosition.y * inInstanceScale;

    // 风力方向（世界空间）
    vec2 windDir = normalize(vec2(push.windDirectionX, push.windDirectionZ));

    // 多频率叠加（更自然的风动效果）
    float wave1 = sin(push.windTime * push.windSpeed * 1.0 + inPosition.y * 5.0) * 0.6;
    float wave2 = sin(push.windTime * push.windSpeed * 1.7 + inPosition.y * 3.0) * 0.3;
    float wave3 = sin(push.windTime * push.windSpeed * 0.5 + inPosition.y * 8.0) * 0.1;
    float wave = wave1 + wave2 + wave3;

    // 弯曲程度随高度增加（树根不动，树冠最弯）
    // smoothstep 让底部更硬，顶部更软
    float bendFactor = smoothstep(0.0, 0.3, inPosition.y) * push.windStrength;

    // 最终风力偏移（仅在XZ平面）
    vec3 windOffset = vec3(windDir.x, 0.0, windDir.y) * wave * bendFactor * worldHeight;

    vec3 worldPos = inInstancePos + offset + windOffset;

    gl_Position = ubo.projectionMatrix * ubo.viewMatrix * vec4(worldPos, 1.0);
    fragPosWorld = worldPos;
    fragNormalWorld = vec3(0.0, 1.0, 0.0);
}
