#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;
layout(location = 3) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

struct PointLight {
    vec4 position;
    vec4 color;      // RGB + intensity
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseView;    // 用于获取相机位置
    vec4 ambientColor;
    int numLights;
    PointLight pointLights[10];
} ubo;

// 材质 UBO (set=1, binding=0)
layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    int hasDiffuseMap;
    int hasNormalMap;
    int hasSpecularMap;   // 在 PBR 中可重用为 metallic-roughness 贴图标志
} material;

// 纹理 (set=1, binding 1~3)
layout(set = 1, binding = 1) uniform sampler2D diffuseSampler;      // Albedo
layout(set = 1, binding = 2) uniform sampler2D normalSampler;       // 法线贴图（切线空间）
layout(set = 1, binding = 3) uniform sampler2D specularSampler;     // 可存储金属度(R) + 粗糙度(G)

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
} push;

// ---------- PBR 辅助函数 ----------
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.1415926 * denom * denom;
    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// 从法线贴图采样并转换到世界空间
vec3 getNormalFromMap(vec3 tangent, vec3 normal, vec2 texCoord) {
    vec3 tangentNormal = texture(normalSampler, texCoord).xyz * 2.0 - 1.0;
    // 简单的 TBN 矩阵（需要物体提供切线，如果没有则用 dFdx 方法）
    vec3 N = normalize(normal);
    vec3 T = normalize(tangent);
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

void main() {
    // ---------- 1. 获取材质参数 ----------
    vec3 albedo = material.baseColorFactor.rgb;
    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;
    float ao = 1.0;   // 环境光遮蔽，暂未提供贴图

    if (material.hasDiffuseMap > 0) {
        albedo *= texture(diffuseSampler, fragTexCoord).rgb;
    }

    // 如果 specularSampler 存储的是 Metal/Roughness 贴图 (R=金属, G=粗糙)
    if (material.hasSpecularMap > 0) {
        vec4 mrTex = texture(specularSampler, fragTexCoord);
        metallic *= mrTex.r;
        roughness *= mrTex.g;
    }

    // 限制 roughness 范围
    roughness = clamp(roughness, 0.04, 1.0);

    // ---------- 2. 法线处理 ----------
    vec3 normal = normalize(fragNormalWorld);
    // 如果你有切线（需在 vertex shader 中传入），这里可以调用 getNormalFromMap
    // 目前假设没有法线贴图，但保留接口
    if (material.hasNormalMap > 0) {
        // 需要切线，若没有则跳过或使用 dFdx 方法，这里简单跳过
        // normal = getNormalFromMap(tangent, normal, fragTexCoord);
    }

    // ---------- 3. 观察方向 ----------
    vec3 V = normalize(ubo.inverseView[3].xyz - fragPosWorld);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);  // 基础反射率

    // ---------- 4. 光照累积 ----------
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < ubo.numLights; i++) {
        PointLight light = ubo.pointLights[i];
        vec3 L = normalize(light.position.xyz - fragPosWorld);
        vec3 H = normalize(V + L);

        float distance = length(light.position.xyz - fragPosWorld);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = light.color.rgb * light.color.w * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(normal, H, roughness);
        float G = GeometrySmith(normal, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= (1.0 - metallic);

        float NdotL = max(dot(normal, L), 0.0);
        Lo += (kD * albedo / 3.1415926 + specular) * radiance * NdotL;
    }

    // ---------- 5. 环境光照（简单常量，之后可替换为 IBL）----------
    vec3 ambient = ubo.ambientColor.rgb * ubo.ambientColor.w * albedo * 0.03;  // 环境光强度很低

    vec3 color = ambient + Lo;

    // ---------- 6. 色调映射 + Gamma 校正 ----------
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}