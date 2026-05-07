#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;
// 可选：法线贴图需要的切线
// layout(location = 4) in vec3 tangent;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec3 fragNormalWorld;
layout(location = 3) out vec2 fragTexCoord;
// layout(location = 4) out vec3 fragTangentWorld;

struct PointLight {
    vec4 position;
    vec4 color;
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseView;
    vec4 ambientColor;
    int numLights;
    PointLight pointLights[10];
} ubo;

// Push constant 仅包含模型变换矩阵
layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
} push;

void main() {
    vec4 worldPos = push.modelMatrix * vec4(position, 1.0);
    gl_Position = ubo.projectionMatrix * ubo.viewMatrix * worldPos;
    fragNormalWorld = normalize(mat3(push.normalMatrix) * normal);
    fragPosWorld = worldPos.xyz;
    fragColor = color;
    fragTexCoord = uv;
    // 如果有切线：fragTangentWorld = normalize(mat3(push.normalMatrix) * tangent);
}