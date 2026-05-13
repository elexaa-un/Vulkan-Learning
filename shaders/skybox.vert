#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 outTexCoord;

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

    // ===== 新增：风力参数（必须与C++端GlobalUbo对齐）=====
    float windTime;
    float windStrength;
    float windSpeed;
    float windDirectionX;
    float windDirectionZ;
    float _pad0;
    float _pad1;
    float _pad2;
} ubo;

layout(push_constant) uniform Push {
    mat4 viewNoTranslation;
} push;

void main() {
    outTexCoord = inPosition;
    // 使用正确的字段名
    vec4 pos = ubo.projectionMatrix * push.viewNoTranslation * vec4(inPosition, 1.0);
    gl_Position = vec4(pos.xy,0.999999,pos.w);
}