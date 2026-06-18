#version 450
// 粒子片元着色器：贴图版本
// 直接输出纹理颜色，利用纹理自带 alpha 通道实现透明

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D particleTex;

void main()
{
    vec4 texColor = texture(particleTex, fragUV);
    // 纹理的 RGB 作为粒子外观，alpha 控制透明度（PNG 透明通道）
    outColor = texColor;
}
