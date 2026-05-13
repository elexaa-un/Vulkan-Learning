# 植被风力系统集成教程

---

## 目录
1. [架构总览：当前数据流分析](#1)
2. [风力系统的核心原理](#2)
3. [方案对比：三种风力参数传递方式](#3)
4. [推荐方案：扩展 GlobalUbo + Shader 风力计算](#4)
5. [Step-by-Step 实施指南](#5)
6. [Shader 风力算法的深入讲解](#6)
7. [高级话题](#7)
8. [调试与常见问题](#8)

---

<a name="1"></a>
## 1. 架构总览：当前数据流分析

在动手之前，我们需要完全理解当前数据是如何从 C++ 端流向 GPU 的。

### 1.1 你的植被渲染管线

```
┌─────────────────────────────────────────────────────────────┐
│                        C++ 端                                │
│                                                             │
│  VegetationInstance          GlobalUbo                       │
│  ┌─────────────────┐        ┌──────────────────────┐        │
│  │ position (vec3) │        │ projection (mat4)     │        │
│  │ scale    (float)│        │ view (mat4)           │        │
│  │         16 bytes │        │ inverseView (mat4)    │        │
│  └────────┬────────┘        │ ambientColor (vec4)   │        │
│           │                 │ numLights (int)       │        │
│           │                 │ pointLights[10]       │        │
│           │                 └───────────┬───────────┘        │
│           │                             │                    │
│  VBO binding=1                 UBO set=0 binding=0           │
│  (per-instance)                (per-frame)                   │
└───────────┼─────────────────────────────┼────────────────────┘
            │                             │
════════════│══════════════════════════════│════════════════════
            │         GPU 端               │
            ▼                             ▼
┌───────────────────────────────────────────────────────────────┐
│              vegetation.vert (顶点着色器)                     │
│                                                               │
│  inInstancePos ─────┐                                         │
│  inInstanceScale ───┤                                         │
│                     ├──? worldPos = inInstancePos + offset    │
│  ubo.view ─────────┤      (广告牌计算)                       │
│  ubo.projection ───┘                                          │
│                                                               │
│  当前没有任何时间/风力参数！                                  │
└───────────────────────────────────────────────────────────────┘
```

### 1.2 关键文件及其职责

| 文件 | 职责 |
|------|------|
| `systems/vegetation_instance.hpp` | 定义每个植被实例的数据结构（position + scale）|
| `systems/lve_vegetation_system.cpp` | 管理实例VBO创建、管线配置、渲染命令 |
| `systems/lve_vegetation_system.hpp` | VegetationSystem类声明 |
| `lve_frame_info.hpp` | 定义 `GlobalUbo` 和 `FrameInfo` 结构体 |
| `first_app.cpp` | 主循环：创建UBO、写入数据、更新ImgUI、每帧填充FrameInfo |
| `build/shaders/vegetation.vert` | 顶点着色器：广告牌+实例化 |
| `build/shaders/vegetation.frag` | 片元着色器：光照+纹理采样 |

---

<a name="2"></a>
## 2. 风力系统的核心原理

### 2.1 植被风动的本质

在实时渲染中，植被风动不是物理模拟，而是**顶点级别的程序化动画**。核心思想是：

> 在顶点着色器中，根据**顶点的高度**和**时间**，对顶点的水平位置施加一个周期性的偏移。

```
无风时：                   有风时：
   │                        ︵
   │ 树叶                   │  树叶偏移
   │                       ╱
   │                      │   树干微微弯曲
   │                      │
 ──┴── 地面             ──┴── 地面（不动）
```

### 2.2 弯曲函数

最基础的风力弯曲公式：

```
offset_x = sin(time * windSpeed + position.y * waveFrequency) * windStrength * position.y
```

- `position.y`：顶点高度（越高越弯，根部为0则不动）
- `time`：全局时间（秒），让风持续变化
- `windSpeed`：风的变化速率
- `waveFrequency`：沿高度的波频（控制弯曲的"节"数）
- `windStrength`：风力强度（振幅）

### 2.3 广告牌植被的特殊性

你的植被使用的是**广告牌（Billboard）技术**——每棵树是一个始终面向相机的四边形。这意味着：

1. 四边形有4个顶点，Y坐标从0（根部）到1*scale（顶部）
2. 风力的偏移方向应该是**水平方向**（垂直于Y轴）
3. 偏移需要在世界空间中计算，但同时要考虑广告牌的朝向

**关键洞察**：你的 `inPosition.y` 在 quad 的局部空间中，范围是 [0, 1]（归一化高度），`inInstanceScale` 决定实际高度。所以实际世界高度 = `inPosition.y * inInstanceScale`。

---

<a name="3"></a>
## 3. 方案对比：三种风力参数传递方式

### 方案 A：扩展 GlobalUbo（★ 推荐）

在现有的 `GlobalUbo` 结构体中添加风力参数。

```
优点：
  ? 简单直接，改动最小
  ? 风力参数每帧自动更新（跟着UBO走）
  ? 所有shader都能访问（天空盒等其他系统也能用）
  
缺点：
  ? GlobalUbo已经很大（~800字节），再加字段会增加带宽
  ? 与光照数据耦合在一起，语义不够清晰
```

### 方案 B：新增独立的 WindUbo（set=0, binding=2）

创建一个新的 UBO 专门存储风力参数。

```
优点：
  ? 职责分离，架构清晰
  ? 可以独立更新（不需要每帧重写整个GlobalUbo）
  ? 可以挂在不同的 shader stage 上
  
缺点：
  ? 需要额外的描述符集布局、描述符池分配、绑定操作
  ? 代码改动较多
```

### 方案 C：使用 Push Constants

通过 Vulkan 的 Push Constants 传递风力参数。

```
优点：
  ? 最快的数据路径（GPU寄存器级别）
  ? 不需要描述符集
  ? 非常适合少量高频更新的数据
  
缺点：
  ? 大小受限（通常128字节，但足够风力参数）
  ? 需要在 pipeline layout 中配置
  ? 每个 draw call 都要 push
```

---

<a name="4"></a>
## 4. 推荐方案：扩展 GlobalUbo + 风力时间加入 FrameInfo

**选择方案A作为首选**，因为：
- 你的项目规模适合简单方案
- GlobalUbo 已经有 `ambientColor` 和 `pointLights`，加一个 `windData` 很自然
- 改动最集中在3个文件

同时，我把风力**时间**放在 `FrameInfo` 中，因为 `frameTime` 可以累加得到总时间。

---

<a name="5"></a>
## 5. Step-by-Step 实施指南

### 5.1 步骤一：修改 `lve_frame_info.hpp` — 扩展 GlobalUbo

在 `GlobalUbo` 结构体中添加风力数据，在 `FrameInfo` 中添加累计时间：

```cpp
// lve_frame_info.hpp

// 在 GlobalUbo 结构体末尾（pointLights 之后）添加：
struct GlobalUbo
{
    glm::mat4 projection{1.f};
    glm::mat4 view{1.f};
    glm::mat4 inverseView{1.f};
    glm::vec4 ambientColor{1.f, 1.f, 1.f, .02f};
    int numLights;
    PointLight pointLights[MAX_LIGHTS];

    // ===== 新增：风力参数 =====
    float windTime;       // 累计时间（秒）
    float windStrength;   // 风力强度 [0.0, 2.0]
    float windSpeed;      // 风速 [0.1, 5.0]
    float windDirectionX; // 风向 X 分量（归一化）
    float windDirectionZ; // 风向 Z 分量（归一化）
    float _pad[3];        // 对齐填充（保证16字节对齐）
};
```

**重要**：`GlobalUbo` 是 uniform buffer，需要严格遵守 `std140` 布局规则：
- `float` 对齐到4字节
- `vec4` / `mat4` 对齐到16字节
- 结构体总大小必须是 `minUniformBufferOffsetAlignment` 的倍数（通常是256字节）

由于我们在末尾加了6个float（24字节）+ 3个pad float（12字节），增加了36字节，整体仍然是16字节对齐的。

### 5.2 步骤二：修改 `systems/vegetation_instance.hpp` — （可选）添加每实例风力系数

如果你想让每棵树有不同的风力响应（比如大树摇得慢，小树摇得快），可以扩展实例数据：

```cpp
// vegetation_instance.hpp
struct VegetationInstance
{
    glm::vec3 position{0.0f};
    float    scale{1.0f};
    float    windFlexibility{1.0f};  // 新增：风力柔韧度 [0.0, 1.0]
    float    _pad[3];                // 对齐填充
};
```

**但这会增加 VBO 大小并需要修改管线配置（binding stride、属性描述）。**
对于入门级实现，**建议先用全局统一的风力参数，跳过此步骤**，后续再扩展。

### 5.3 步骤三：修改 `first_app.cpp` — 填入风力数据

在 `first_app.cpp` 的 `run()` 函数主循环中，找到填充 `GlobalUbo` 的代码段：

```cpp
// first_app.cpp — run() 函数中，大约第140行附近

// 原代码：
GlobalUbo ubo{};
ubo.projection = camera.getProjection();
ubo.view = camera.getView();
ubo.inverseView = camera.getInverseView();
pointLightSystem->update(frameInfo, ubo);
uboBuffers[frameIndex]->writeToBuffer(&ubo);
uboBuffers[frameIndex]->flush();

// 修改为：
GlobalUbo ubo{};
ubo.projection = camera.getProjection();
ubo.view = camera.getView();
ubo.inverseView = camera.getInverseView();
pointLightSystem->update(frameInfo, ubo);

// ===== 新增：风力参数 =====
// 从ImgUI获取风力参数（需要在main loop外部声明static变量）
// 或者通过某种方式传入

uboBuffers[frameIndex]->writeToBuffer(&ubo);
uboBuffers[frameIndex]->flush();
```

现在的问题是：ImgUI的 `windStrength` 和 `windSpeed` 变量是 `static` 局部变量，在 ImGui::Begin 的作用域内。我们需要把它们提取出来。

**方案：将风力变量提升为 `FirstApp` 的成员变量，或作为 `run()` 中的局部变量但在 ImGui 作用域外声明。**

推荐结构：

```cpp
void FirstApp::run()
{
    // ... existing code ...

    // ==== 风力参数（声明在 ImGui 作用域外，以便写入 UBO）====
    float windStrength = 0.5f;
    float windSpeed = 1.0f;
    float totalTime = 0.0f;  // 累计时间

    while (!lveWindow.shouldClose())
    {
        glfwPollEvents();
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime = newTime;
        frameTime = glm::min(frameTime, MAX_FRAME_TIME);

        totalTime += frameTime;  // 累计时间

        // ... camera update ...

        if (auto commandBuffer = lveRenderer.beginFrame())
        {
            // ... ImGui::Begin ...

            // ImGui 内部引用外部变量（需要用指针或引用）
            ImGui::SliderFloat("Wind Strength", &windStrength, 0.0f, 2.0f);
            ImGui::SliderFloat("Wind Speed", &windSpeed, 0.1f, 5.0f);

            // ... ImGui::End ...

            GlobalUbo ubo{};
            ubo.projection = camera.getProjection();
            ubo.view = camera.getView();
            ubo.inverseView = camera.getInverseView();
            pointLightSystem->update(frameInfo, ubo);

            // ===== 写入风力参数 =====
            ubo.windTime = totalTime;
            ubo.windStrength = windStrength;
            ubo.windSpeed = windSpeed;
            // 风向：可以从风向来推导方向向量
            // 简单起见，用固定的方向（例如沿X轴）
            ubo.windDirectionX = 1.0f;
            ubo.windDirectionZ = 0.0f;

            uboBuffers[frameIndex]->writeToBuffer(&ubo);
            uboBuffers[frameIndex]->flush();

            // ... rendering ...
        }
    }
}
```

**关键点**：把 ImGui 中原本的 `static float windStrength` / `static float windSpeed` 删除，改为引用外部的 `windStrength` 和 `windSpeed`。

原来是：
```cpp
static float windStrength = 0.5f;
static float windSpeed = 1.0f;
ImGui::SliderFloat("Wind Strength", &windStrength, 0.0f, 2.0f);
ImGui::SliderFloat("Wind Speed", &windSpeed, 0.1f, 5.0f);
```

改为：
```cpp
ImGui::SliderFloat("Wind Strength", &windStrength, 0.0f, 2.0f);
ImGui::SliderFloat("Wind Speed", &windSpeed, 0.1f, 5.0f);
```

### 5.4 步骤四：修改 `systems/lve_vegetation_system.cpp` — 更新管线布局（如需扩展实例）

如果你跳过了步骤二（不扩展 VegetationInstance），则**无需修改此文件**。

如果你扩展了 VegetationInstance（增加了 windFlexibility），需要修改：

1. `createPipeline()` 中的 binding stride：
   ```cpp
   bindings[1].stride = sizeof(VegetationInstance); // 自动适应新大小
   ```

2. 添加新的顶点属性描述（location=6）：
   ```cpp
   VkVertexInputAttributeDescription windFlexAttr{};
   windFlexAttr.location = 6;
   windFlexAttr.binding = 1;
   windFlexAttr.format = VK_FORMAT_R32_SFLOAT;
   windFlexAttr.offset = offsetof(VegetationInstance, windFlexibility);
   attributes.push_back(windFlexAttr);
   ```

### 5.5 步骤五：修改 `build/shaders/vegetation.vert` — 核心风力计算

这是**最重要的步骤**。在顶点着色器的 `main()` 函数中，在计算 `worldPos` 之前加入风力偏移。

```glsl
// vegetation.vert

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
    mat4 projection;
    mat4 view;
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

void main() {
    fragUV = inUV;

    vec3 cameraRight = ubo.inverseView[0].xyz;
    vec3 cameraUp    = ubo.inverseView[1].xyz;
    cameraUp = vec3(0.0, 1.0, 0.0);  // 保持直立

    // ===== 广告牌基础偏移 =====
    vec3 offset = cameraRight * inPosition.x * inInstanceScale
                + cameraUp    * inPosition.y * inInstanceScale;

    // ===== 新增：风力弯曲偏移 =====
    // inPosition.y 是归一化高度 [0, 1]
    // 实际世界高度 = inPosition.y * inInstanceScale
    float worldHeight = inPosition.y * inInstanceScale;

    // 风力方向（世界空间）
    vec2 windDir = normalize(vec2(ubo.windDirectionX, ubo.windDirectionZ));

    // 多频率叠加（更自然的风动效果）
    float wave1 = sin(ubo.windTime * ubo.windSpeed * 1.0 + inPosition.y * 5.0) * 0.6;
    float wave2 = sin(ubo.windTime * ubo.windSpeed * 1.7 + inPosition.y * 3.0) * 0.3;
    float wave3 = sin(ubo.windTime * ubo.windSpeed * 0.5 + inPosition.y * 8.0) * 0.1;
    float wave = wave1 + wave2 + wave3;

    // 弯曲程度随高度增加（根部不动，顶部最弯）
    // 使用 smoothstep 让底部更硬，顶部更软
    float bendFactor = smoothstep(0.0, 0.3, inPosition.y) * ubo.windStrength;

    // 最终风力偏移（仅在XZ平面）
    vec3 windOffset = vec3(windDir.x, 0.0, windDir.y) * wave * bendFactor * worldHeight;

    vec3 worldPos = inInstancePos + offset + windOffset;

    gl_Position = ubo.projection * ubo.view * vec4(worldPos, 1.0);
    fragPosWorld = worldPos;
    fragNormalWorld = vec3(0.0, 1.0, 0.0);
}
```

### 5.6 步骤六：修改 `build/shaders/vegetation.frag` — 同步Ubo定义

片元着色器中的 `GlobalUbo` 定义也必须添加风力字段（即使不使用），否则布局不匹配：

```glsl
// vegetation.frag — GlobalUbo 部分

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    vec4 ambientColor;
    int numLights;
    PointLight pointLights[10];
    // ===== 新增（占位，保持与vert一致）=====
    float windTime;
    float windStrength;
    float windSpeed;
    float windDirectionX;
    float windDirectionZ;
    float _pad0;
    float _pad1;
    float _pad2;
} ubo;
```

### 5.7 步骤七：重新编译Shader

在 `build/shaders/` 目录下，使用 glslc 编译更新后的 shader：

```bash
cd e:\Vulkan-Learning\build\shaders
glslc vegetation.vert -o vegetation.vert.spv
glslc vegetation.frag -o vegetation.frag.spv
```

如果你的 `compile.bat` 中包含了编译命令，直接运行它即可。

### 5.8 步骤八：重新编译C++项目

```bash
cd e:\Vulkan-Learning\build
cmake --build .
```

---

## 5. 修改清单总结

| 文件 | 修改内容 | 难度 |
|------|---------|------|
| `lve_frame_info.hpp` | GlobalUbo添加6个float风力字段 + 3个pad | ★☆☆ |
| `first_app.cpp` | 提取wind变量到外部，填充ubo.windXXX，移除static | ★★☆ |
| `build/shaders/vegetation.vert` | 添加风力UBO字段声明 + 风力偏移计算 | ★★★ |
| `build/shaders/vegetation.frag` | 添加风力UBO字段声明（占位） | ★☆☆ |
| (可选) `vegetation_instance.hpp` | 添加windFlexibility | ★★☆ |
| (可选) `lve_vegetation_system.cpp` | 添加location=6属性 | ★★☆ |

---

<a name="6"></a>
## 6. Shader 风力算法的深入讲解

### 6.1 为什么使用多频率叠加？

单一正弦波会让所有植物同步摆动，看起来像"军队"。自然界中，不同高度的树枝摆动频率不同。

```
wave1: 主波 — 低频大振幅 — 模拟整体树干摇摆
wave2: 次波 — 中频中振幅 — 模拟枝干摆动
wave3: 细波 — 高频小振幅 — 模拟叶片颤动
```

叠加后的效果更自然。

### 6.2 bendFactor 的作用

```glsl
float bendFactor = smoothstep(0.0, 0.3, inPosition.y) * ubo.windStrength;
```

- `smoothstep(0.0, 0.3, inPosition.y)`：在 quad 底部 0~30% 高度范围内，弯曲系数从0平滑过渡到1
- 这意味着根部附近的顶点几乎不受风力影响（模拟根部固定在土壤中）
- 乘以 `windStrength` 让ImGui滑块可以全局控制

### 6.3 为什么用 `worldHeight` 而不是直接 `inPosition.y`？

`inPosition.y` 范围是 [0, 1]（quad的归一化高度）。不同树的 scale 不同（大树 vs 小树），如果用归一化高度直接乘风力，大树和小树弯曲量一样（在归一化空间）。乘以 `inInstanceScale` 后，大树弯曲更大（符合物理直觉）。

### 6.4 风向的实现

```glsl
vec2 windDir = normalize(vec2(ubo.windDirectionX, ubo.windDirectionZ));
vec3 windOffset = vec3(windDir.x, 0.0, windDir.y) * wave * bendFactor * worldHeight;
```

风向是一个2D向量（XZ平面），可以：
- 固定方向（如(1,0) = 东风吹）
- 随时间旋转（如 `(cos(time), sin(time))` = 旋风）
- 从噪声纹理采样（最真实但最复杂）

---

<a name="7"></a>
## 7. 高级话题

### 7.1 为不同类型植物设置不同的风力响应

如果你有草、灌木、大树，它们应该有不同风力行为：

- **草**：高频小振幅（快速抖动）
- **灌木**：中频中振幅
- **大树**：低频大振幅（缓慢摇摆）

实现方式：
1. 扩展 `VegetationInstance` 添加 `windType`（int）或 `windFlexibility`
2. 在shader中根据类型使用不同的参数

### 7.2 使用噪声函数替代正弦波

正弦波的问题是可预测、重复。Perlin噪声或Simplex噪声能产生更自然的风动。

在shader中实现2D/3D Simplex噪声（约50行GLSL代码），将 `time` 和 `worldPos.xz` 作为输入：

```glsl
float windNoise = snoise(vec3(worldPos.xz * 0.1, ubo.windTime * 0.5));
windOffset *= windNoise;
```

这样每棵树的摆动幅度都不同（根据世界坐标），看起来更自然。

### 7.3 添加风力对法线的影响

如果风力弯曲很大，可以做简单的法线扰动来改善光照：

```glsl
// 计算弯曲后的近似法线
vec3 bentNormal = normalize(vec3(-windOffset.x, 1.0, -windOffset.z));
fragNormalWorld = bentNormal;
```

但这需要谨慎——对于广告牌，法线通常是向上的，法线扰动可能会让光照看起来奇怪。

### 7.4 LOD：远处的植物降低风力计算精度

如果场景中有大量植被实例（>10000），考虑在远处使用简化的风力计算甚至跳过。

---

<a name="8"></a>
## 8. 调试与常见问题

### 8.1 Shader编译错误：layout mismatch

如果你看到类似以下错误：
```
error: 'GlobalUbo' : structure is not the same size as the C++ structure
```

**原因**：GLSL和C++端的 GlobalUbo 字段数量/顺序/类型不一致。

**解决**：
1. 检查C++端 `GlobalUbo` 的字段顺序
2. 确保GLSL中 `GlobalUbo` 的字段顺序完全一致
3. 注意 `alignas(16)` 的 padding
4. 可以用 `static_assert(sizeof(GlobalUbo) == expected_size)` 验证

### 8.2 植物完全不摆动

**可能原因**：
1. Shader没有被重新编译（`.spv` 文件是旧的）
2. `ubo.windStrength = 0`（ImGui滑块为0）
3. `totalTime` 没有累加
4. `windTime` 没有写入UBO

**检查点**：
- 在ImGui中把Wind Strength调到2.0
- 检查shader编译时间戳
- 在C++端打印 `ubo.windTime` 确认值在变化

### 8.3 植物摆动方向不对

检查 `windDirectionX` 和 `windDirectionZ` 的值。如果填反了，方向就会错误。

### 8.4 对齐/UBO大小问题导致渲染异常

Vulkan的 uniform buffer 对齐要求很严格。如果添加字段后总大小不是 `minUniformBufferOffsetAlignment` 的倍数，可能导致整个UBO读取错误。

你的 `GlobalUbo` 当前大小大约是：
```
mat4 projection:    64 bytes
mat4 view:          64 bytes  
mat4 inverseView:   64 bytes
vec4 ambientColor:  16 bytes
int numLights:       4 bytes
PointLight[10]:    320 bytes (每个32字节)
────────────────────────
合计:              532 bytes
```

添加6个float（24字节）后 = 556字节，加上3个pad float = 568字节。

确认 `minUniformBufferOffsetAlignment`（通常是256），568不是256的倍数，所以UBO实际分配的大小会被对齐到 768 字节（3 × 256）。这没问题，Vulkan会自动处理padding。

### 8.5 性能考虑

- 风力计算只用了几次 `sin()` 和乘法，在现代GPU上开销极小
- 每顶点多 ~20条指令，对于1000实例×4顶点 = 4000次顶点计算，影响可以忽略不计
- `sin()` 在GPU上大约4-8个时钟周期

---

## 总结

通过以上7个步骤，你将为植被系统添加完整的风力支持：

1. 在 `GlobalUbo` 中添加风力参数
2. 在主循环中累加时间并填充风力UBO
3. 在顶点着色器中实现多频率叠加弯曲
4. ImGui提供实时调节Wind Strength和Wind Speed

整个改动的架构影响：
```
ImGui Sliders ──? windStrength/windSpeed (C++ float)
                          │
              totalTime ──┤
                          ▼
                GlobalUbo.windTime   ──┐
                GlobalUbo.windStrength ─┤
                GlobalUbo.windSpeed   ──┤  UBO set=0 binding=0
                GlobalUbo.windDirX   ──┤
                GlobalUbo.windDirZ   ──┘
                          │
                          ▼
              vegetation.vert (顶点着色器)
                          │
                  风力偏移计算
                          │
                          ▼
                worldPos += windOffset
```

这份教程涵盖了从原理到实现的每一步，你可以按顺序逐步实施。如果遇到任何问题，欢迎随时询问。
