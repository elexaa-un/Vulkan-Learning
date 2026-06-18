#version 450
// 雨滴粒子顶点着色器：广告牌渲染
// 每个粒子渲染为一个始终面向摄像机的四边形

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;

layout(location = 4) in vec3 inInstancePos;
layout(location = 5) in vec3 inInstanceColor;
layout(location = 6) in float inInstanceSize;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec3 fragColor;

struct PointLight {
    vec4 position;
    vec4 color;
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseView;
    mat4 lightViewProj;
    vec4 ambientColor;
    int numLights;
    PointLight pointLights[10];
} ubo;

void main() {
    fragUV = inUV;
    fragColor = inInstanceColor;

    vec3 cameraRight = vec3(ubo.viewMatrix[0][0], ubo.viewMatrix[1][0], ubo.viewMatrix[2][0]);
    vec3 cameraUp    = vec3(ubo.viewMatrix[0][1], ubo.viewMatrix[1][1], ubo.viewMatrix[2][1]);

    vec3 worldPos = inInstancePos;

    vec3 billboardOffset = cameraRight * inPosition.x * inInstanceSize
                         + cameraUp    * inPosition.y * inInstanceSize;

    vec3 finalWorldPos = worldPos + billboardOffset;
    gl_Position = ubo.projectionMatrix * ubo.viewMatrix * vec4(finalWorldPos, 1.0);
}