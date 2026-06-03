#version 450

// ============================================================
// 全屏后处理 - 片段着色器
// ============================================================
// 作用：采样离屏渲染的场景颜色纹理，对其逐像素应用后处理效果。
//
// 当前实现的效果：
//   1. Reinhard 色调映射（HDR → LDR）
//   2. Gamma 校正
//   3. 饱和度调整
//
// 可扩展：只需修改此着色器即可添加更多效果
//   - 高斯模糊
//   - Bloom
//   - Vignette 暗角
//   - 色彩分级（LUT）
//   - FXAA 抗锯齿
// ============================================================

layout(set = 0, binding = 0) uniform sampler2D sceneColor;  // 离屏颜色纹理输入

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

// 后处理参数（可通过 Uniform Buffer 扩展为可实时调整）
// 这里硬编码：后续可接 UBO 让 ImGui 调整
const float saturation = 1.2;   // 饱和度倍率（>1 增强，<1 减弱）
const float exposure = 1.0;     // 曝光度
const float gamma = 2.2;        // Gamma 值

void main()
{
    // 1. 从离屏颜色纹理采样线性 HDR 颜色
    vec3 hdrColor = texture(sceneColor, fragTexCoord).rgb;

    // 2. 曝光调整
    hdrColor *= exposure;

    // 3. Reinhard 色调映射（HDR → LDR）
    //    公式：color / (color + 1.0)
    //    优点：平滑压缩高亮区域，保留暗部细节
    vec3 mapped = hdrColor / (hdrColor + vec3(1.0));

    // 4. 饱和度调整
    float gray = dot(mapped, vec3(0.299, 0.587, 0.114));
    mapped = mix(vec3(gray), mapped, saturation);

    // 5. 不需要手动 Gamma 校正！
    //    SwapChain 图像格式为 VK_FORMAT_B8G8R8A8_SRGB，
    //    GPU 在写入颜色附件时会自动将线性值转换为 sRGB。
    //    如果在这里手动做 pow(color, 1/2.2)，会导致双重 Gamma 校正，
    //    画面灰暗、对比度低。

    // 6. 输出线性值到 SwapChain（GPU 自动进行 sRGB 转换）
    outColor = vec4(mapped, 1.0);
}
