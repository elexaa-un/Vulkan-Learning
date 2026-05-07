#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;

layout(location = 0) out vec4 outColor;

// 植物纹理 (set=1)
layout(set = 1, binding = 0) uniform sampler2D plantTexture;

// ---------- UBO 定义（必须与 C++ GlobalUbo 完全一致） ----------
struct PointLight {
    vec4 position;   // world position
    vec4 color;      // rgb + intensity
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
    // 1. 采样纹理，进行 Alpha 测试
    vec2 uv = vec2(fragUV.x, 1.0 - fragUV.y);
    vec4 texColor = texture(plantTexture, uv);
    if (texColor.a < 0.5) discard;

    // 2. 法线处理（归一化）
    vec3 normal = normalize(fragNormalWorld);
    // 注意：植物通常双面渲染，如果背面光照错误，可以启用双面光照或翻转背面法线
    // 如果相机在背面，可以翻转法线（简化：不处理也能凑合）

    // 3. 环境光分量（使用 ubo.ambientColor）
    vec3 ambient = ubo.ambientColor.rgb * ubo.ambientColor.w * texColor.rgb;

    // 4. 漫反射光照累加
    vec3 diffuseSum = vec3(0.0);
    for (int i = 0; i < ubo.numLights; i++) {
        PointLight light = ubo.pointLights[i];
        vec3 lightDir = normalize(light.position.xyz - fragPosWorld);
        float diff = max(dot(normal, lightDir), 0.0);
        // 光源颜色 * 强度
        vec3 lightColor = light.color.rgb * light.color.w;
        // 简单的衰减：距离平方倒数
        float distance = length(light.position.xyz - fragPosWorld);
        float attenuation = 1.0 / (distance * distance + 1.0); // 避免除零
        diffuseSum += diff * lightColor * attenuation;
    }

    // 5. 最终颜色 = (环境光 + 漫反射) * 纹理颜色
    vec3 finalColor = (ambient + diffuseSum) * texColor.rgb;

    outColor = vec4(finalColor, texColor.a);
}