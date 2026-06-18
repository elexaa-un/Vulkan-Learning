#version 450
// 雨滴粒子片元着色器：贴图采样

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D particleTex;

void main()
{
    outColor = texture(particleTex, fragUV);
}