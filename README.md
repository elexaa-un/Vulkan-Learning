# Vulkan-Learning

> 一个基于 Vulkan API 的实时 3D 渲染引擎学习项目，支持 PBR 材质、动态阴影、视锥体剔除、GPU 实例化植被（风力动画）、天空盒、ImGui 调试面板等功能。

---

## 目录

- [项目简介](#项目简介)
- [核心特性](#核心特性)
- [项目架构](#项目架构)
- [模块详解](#模块详解)
- [构建与运行](#构建与运行)
- [操作说明](#操作说明)
- [项目优缺点分析](#项目优缺点分析)
- [下一步目标](#下一步目标)
- [依赖](#依赖)

---

## 项目简介

**Vulkan-Learning** 是一个从零开始构建的 Vulkan 实时渲染引擎，旨在学习 Vulkan API 的底层运作机制。项目采用 C++17 编写，使用 CMake 构建系统，渲染核心自研，仅依赖于 GLFW（窗口）、Vulkan SDK 和 ImGui（调试界面）。项目目前支持完整的 3D 场景渲染，包含地形、模型、植被、天空盒、动态光源、实时阴影和视锥体剔除。

---

## 核心特性

| 特性 | 说明 |
|------|------|
| **Vulkan 底层封装** | 自研 `lve_*` 系列类，封装设备、交换链、管线、缓冲区、纹理、描述符等核心概念 |
| **PBR 材质系统** | 支持漫反射、法线、高光贴图，可配置金属度/粗糙度 |
| **地形系统** | 基于高度图自动生成地形网格 + 纹理采样（256x256 分段） |
| **动态点光源** | 最多支持 10 个点光源，可实时调节位置/强度/颜色 |
| **Shadow Mapping** | 2048x2048 深度阴影贴图，支持实时阴影投射 |
| **天空盒** | 立方体贴图天空盒渲染 |
| **植被系统** | GPU 实例化渲染（3000 实例）+ 广告牌 + 程序化风力动画（Push Constants 传参） |
| **视锥体剔除** | AABB + 视锥体平面检测，自动跳过视口外物体，ImGui 实时显示剔除统计 |
| **ImGui 调试面板** | 实时显示 FPS、摄像机参数、光源调节、风力强度、渲染选项开关、剔除统计 |
| **自由摄像机** | WASD 移动 + J/K 升降 + E/Q 旋转 + 方向键视角 + 鼠标右键拖拽旋转 + 滚轮缩放 |
| **统一资源管理器** | `LveResourceManager` 管理模型/纹理/材质/描述符布局的加载、缓存与生命周期 |
| **Fallback 纹理** | `LveTextureManager` 提供 1x1 白色纹理 + 默认采样器的兜底机制 |
| **着色器自动编译** | CMake 集成 `glslc`，修改 `.vert/.frag` 后自动编译为 SPIR-V（`CompileShaders` target） |

---

## 项目架构

```
+---------------------------------------------------------+
|                      main.cpp                            |
|                    (程序入口)                              |
+--------------------------+--------------------------------+
                       |
                       v
+----------------------------------------------------------+
|                    FirstApp (主控类)                       |
|  . 持有所有引擎组件                                         |
|  . 管理渲染循环 (run)                                      |
|  . 加载场景 (LoadGameObjects)                              |
|  . 协调各渲染系统                                           |
|  . 拆分为独立初始化函数 (initGlobalResources,               |
|    initShadowResources, initVegetationSystem,              |
|    initRenderSystems)                                      |
+-------+--------------------------------------------------+
        |
        | 持有/创建
        +---> LveWindow             (GLFW window)
        +---> LveDevice             (Vulkan device)
        +---> LveRenderer           (frame renderer)
        |       +---> LveSwapChain  (swap chain + RenderPass)
        +---> LveResourceManager    (resource cache manager)
        +---> LveCamera             (camera)
        +---> LveImgui              (ImGui integration)
        |
        | render systems (systems/)
        +---> LveSimpleRenderSystem    (PBR model rendering + frustum culling)
        +---> LvePointLightSystem      (point light rendering)
        +---> LveSkyboxSystem          (skybox rendering)
        +---> LveShadowSystem          (shadow map generation)
        +---> LveVegetationSystem      (vegetation instanced rendering)
        |
        | core engine (lve_*)
        +---> LveModel              (model loading / vertex buffer + AABB)
        +---> LveTexture            (texture / cubemap)
        +---> LveMaterial           (material management)
        +---> LveGameObject         (game object + ECS prototype)
        +---> LvePipeline           (graphics pipeline wrapper)
        +---> LveBuffer             (VBO / UBO management)
        +---> LveDescriptors        (descriptor set layout / pool / writer)
        +---> LveFrameInfo          (frame data: GlobalUbo + FrameInfo)
        +---> Frustum / AABB        (frustum culling geometry utils)
        +---> KeyboardMovementController (camera controller)
```

### 渲染管线流程

```
per-frame execution order:

1. beginFrame()                    -> acquire CommandBuffer + ImageIndex
2. Shadow Pass                     -> render depth map from light's perspective
3. beginSwapChainRenderPass()      -> begin main render pass
4. SkyboxSystem.render()           -> draw skybox (optional)
5. SimpleRenderSystem.render()     -> draw PBR models (frustum culling, optional)
6. VegetationSystem.render()       -> draw vegetation instances 3000 trees (optional)
7. PointLightSystem.render()       -> draw light visualization spheres
8. ImGui.render()                  -> draw debug panel
9. endSwapChainRenderPass()        -> end render pass
10. endFrame()                     -> present + submit
```

### 描述符集布局

```
Set = 0 (Global Descriptor Set):
  binding = 0: GlobalUbo          (Uniform Buffer)
  binding = 1: Cubemap Sampler    (skybox texture)
  binding = 2: Shadow Map Sampler (shadow map)

Set = 1 (Material Descriptor Set):
  binding = 0: Material UBO       (material parameters)
  binding = 1: Diffuse Texture    (diffuse map)
  binding = 2: Normal Texture     (normal map)
  binding = 3: Specular Texture   (specular map)

Set = 2 (Vegetation Texture Set):
  binding = 0: Tree Texture       (vegetation texture)

Wind parameters via Push Constants (no longer in UBO):
  windTime / windStrength / windSpeed / windDirectionX / windDirectionZ
```

---

## 模块详解

### 引擎层 (Engine Core)

#### `LveWindow` -- 窗口管理
- 基于 GLFW 封装窗口创建、事件轮询
- 回调处理窗口大小调整
- 暴露 `VkSurfaceKHR` 创建接口

#### `LveDevice` -- Vulkan 设备封装
- 物理设备选择（GPU 选择逻辑）
- 逻辑设备创建 + 队列族选择（Graphics + Present）
- 命令池管理
- 设备属性查询

#### `LveSwapChain` -- 交换链
- 三重缓冲交换链创建/重建
- 深度缓冲区管理
- RenderPass 封装
- 窗口大小变化自动重建

#### `LveRenderer` -- 帧渲染控制
- 单帧生命周期管理 (beginFrame/endFrame)
- CommandBuffer 分配与回收
- 多帧同步 (Fence/Semaphore)
- 交换链重建触发

#### `LvePipeline` -- 图形管线
- Shader 模块加载 (SPIR-V)
- 管线状态配置 (深度测试、混合、背面剔除等)
- 支持可配置的顶点输入 + 实例化

#### `LveBuffer` -- 缓冲区管理
- VBO / IBO / UBO 统一封装
- Staging Buffer 模式（设备本地内存）
- Host-Visible 内存映射
- 对齐约束处理

#### `LveTexture` -- 纹理管理
- 2D 纹理加载 (stb_image)
- Mipmap 生成
- 立方体贴图创建（天空盒）
- 采样器配置

#### `LveDescriptors` -- 描述符系统
- **Builder 模式**: `LveDescriptorSetLayout` / `LveDescriptorPool` 均提供 Builder
- **Writer 模式**: `LveDescriptorWriter` 链式写入 + 一次性构建
- 支持增量覆盖 (overwrite)

#### `LveModel` -- 模型系统
- OBJ 文件加载 (tinyobjloader)
- 顶点格式: Position + Color + Normal + UV
- Builder 模式创建
- 静态几何体生成 (天空盒、地形)
- 内建 AABB 包围盒计算（用于视锥体剔除）

#### `LveGameObject` -- 游戏对象
- ECS 雏形：Transform + PointLight 组件
- `std::unordered_map<id, GameObject>` 管理
- 关联 Model / Material / Texture

#### `LveMaterial` -- 材质系统
- PBR 材质参数: BaseColor / Metallic / Roughness
- 三张贴图槽 (Diffuse / Normal / Specular)
- 材质管理器 (`LveMaterialManager`) 工厂模式
- 统一的 buildDescriptorSet() 写入流程

#### `LveCamera` -- 摄像机
- 透视投影 (可配置 FOV / Aspect / Near / Far)
- 基于 YXZ Tait-Bryan 角度的旋转
- View / Projection / InverseView 矩阵

#### `LveResourceManager` -- 统一资源管理器
- 模型缓存: `loadModel()` / `getModel()` / `evictModel()`
- 纹理缓存: `loadTexture()` / `loadCubemap()` / `loadEquirectangularCubemap()`
- 材质管理: `createMaterial()` / `buildAllMaterials()` (整合 LveMaterialManager)
- DescriptorSetLayout 缓存: 基于 Builder 哈希自动去重
- DescriptorPool 托管: `createDescriptorPool()` / `getDescriptorPool()`
- 统计信息: `getStats()` 返回 GPU 内存估算

#### `LveTextureManager` -- Fallback 纹理管理
- 静态类，提供 1x1 白色 RGBA8 纹理 + 默认采样器
- 在贴图加载失败时可作为兜底方案，避免管线崩溃

#### `LveFrameInfo` -- 帧数据结构
- `FrameInfo`: 帧元数据 (frameIndex, frameTime, commandBuffer, camera, descriptorSet, gameObjects, culledCount, totalCount)
- `GlobalUbo`: 全局 UBO (projection, view, inverseView, lightViewProj, ambientColor, pointLights[10])
- 风力参数已从 GlobalUbo 分离，使用独立的 `WindPushConstantData` 通过 Push Constants 传递

---

### 渲染系统层 (Render Systems)

#### `LveSimpleRenderSystem` -- 主渲染系统
- 使用 `simple_shader` (vert + frag)
- 支持 PBR 材质的两级描述符集 (Global + Material)
- **集成了视锥体剔除**: 每帧从 Camera VP 矩阵提取 Frustum，对每个模型计算世界空间 AABB，跳过视锥体外物体
- 剔除统计写入 `FrameInfo.culledCount / totalCount`，由 ImGui 面板展示

#### `LvePointLightSystem` -- 点光源系统
- 使用 `point_light` shader
- 将点光源渲染为发光球体
- 每帧从 gameObjects 收集光源数据写入 GlobalUbo

#### `LveSkyboxSystem` -- 天空盒系统
- 使用 `skybox` shader
- 渲染单位立方体 + cubemap 采样
- 深度测试策略：`GREATER_OR_EQUAL` (始终在场景后方)

#### `LveShadowSystem` -- 阴影系统
- 独立的 Shadow RenderPass (只写深度)
- 2048x2048 深度贴图
- 正交投影 (从光源方向)
- 在主着色器中采样 Shadow Map

#### `LveVegetationSystem` -- 植被系统
- 使用 `vegetation` shader (广告牌 + 实例化)
- GPU 实例化渲染 (3000 实例，per-instance: position + scale)
- 顶点着色器风力动画 (多频率正弦波叠加)
- 风力参数通过 Push Constants 传递（独立于 GlobalUbo）
- 支持 ImGui 实时调节风力参数
- 地形高度采样器 (`TerrainHeightSampler`) 自动贴合地表

---

### 着色器 (Shaders)

| 着色器 | 用途 | Vertex | Fragment |
|--------|------|--------|----------|
| `simple_shader` | PBR 模型渲染 + 阴影采样 | Y | Y |
| `point_light` | 点光源可视化 | Y | Y |
| `skybox` | 天空盒渲染 | Y | Y |
| `shadow` | 阴影深度写入 | Y | Y (空) |
| `vegetation` | 植被广告牌+风动 | Y | Y |

---

## 构建与运行

### 前置依赖

- **Vulkan SDK** (>= 1.3): [LunarG Vulkan SDK](https://vulkan.lunarg.com/)
- **GLFW**: 项目中已包含预编译库 (`glfw/lib/`)
- **GLM**: 头文件库 (通过 CMake 查找或 vcpkg 安装)
- **CMake** (>= 3.20)
- **C++17 编译器** (MSVC 或 MinGW)
- **GPU**: 支持 Vulkan 的显卡

### 构建步骤

```bash
# 1. 确保 Vulkan SDK 已安装并设置环境变量 VULKAN_SDK

# 2. 创建构建目录
mkdir build
cd build

# 3. CMake 配置
cmake ..

# 4. 编译 (着色器会自动编译为 .spv)
cmake --build .

# 5. 运行
./Vulkan_Learning.exe
```

> 项目已配置 POST_BUILD 自动复制资源文件（skybox/、apple/、tree.png、tt.jpg）到构建目录。
> 着色器源文件 (`.vert` / `.frag`) 修改后，CMake 会自动调用 `glslc` 重新编译为 `.spv`。

### 快捷编译脚本

项目根目录提供 `compile.bat`：

```batch
@echo off
cd build
cmake --build .
pause
```

### VSCode 配置

`.vscode/launch.json` 已预置，使用 CMake Tools 扩展 + Ninja 生成器即可直接 F5 调试。

---

## 操作说明

| 操作 | 按键 |
|------|------|
| 前后左右移动 | W / A / S / D |
| 升降 | J / K |
| 左右旋转 | E / Q |
| 视角方向 | Up / Down / Left / Right (arrow keys) |
| 旋转视角 | 鼠标右键拖拽 |
| 缩放视野 | 滚轮 |
| 退出程序 | ESC |
| 切换调试面板 | F1 |
| 调节风力强度 | ImGui -> Vegetation -> Wind Strength |
| 调节光源参数 | ImGui -> Lighting |
| 切换渲染组件 | ImGui -> Render Options (Skybox / Terrain / Model / Vegetation / Wireframe) |

---

## 项目优缺点分析

### 优点

1. **Vulkan 概念覆盖全面**
   - 项目覆盖了 Vulkan 核心概念：设备管理、交换链、管线、描述符、缓冲区、同步等，是对 Vulkan 学习的良好实践

2. **模块化引擎设计良好**
   - `lve_*` 前缀的引擎模块职责清晰：Device、Window、Renderer、Pipeline、Buffer、Texture 等各司其职
   - Builder 模式广泛用于构造复杂对象（DescriptorSetLayout、DescriptorPool、Model）
   - `first_app.cpp` 已将 `run()` 拆分为独立初始化函数，主循环逻辑清晰可读

3. **多渲染系统并存**
   - 5 个独立的渲染系统共享全局 UBO 和描述符集，展示了多 Pass 渲染的架构能力
   - Shadow Mapping 的独立 RenderPass + Framebuffer 设计合理

4. **现代化图形特性**
   - PBR 材质管线（BaseColor / Metallic / Roughness）
   - GPU 实例化植被渲染（3000 实例）
   - 程序化风力动画（顶点着色器计算 + Push Constants 传参，不再占用 UBO）
   - 立方体贴图天空盒
   - 视锥体剔除（AABB + Frustum Plane 检测，ImGui 面板实时查看剔除/总数比例）

5. **实时调试能力**
   - ImGui 集成提供丰富的运行时参数调节：FPS 显示、光源控制、风力调节、渲染选项开关、剔除统计、摄像机重置

6. **良好的资源管理**
   - `LveResourceManager` 统一管理模型/纹理/材质的加载、缓存和生命周期
   - `LveTextureManager` 提供 fallback 纹理兜底机制
   - CMake POST_BUILD 自动复制资源，开发体验友好
   - 使用 `std::unique_ptr` / `std::shared_ptr` 管理 Vulkan 资源生命周期

7. **自动化着色器编译**
   - CMake 集成 `glslc`：通过 `add_custom_command` + `CompileShaders` target 自动检测并编译着色器
   - 主程序通过 `add_dependencies` 确保编译前着色器已是最新

### 缺点

1. **ECS 系统不完整**
   - `LveGameObject` 仅支持 Transform + PointLight 组件，缺少 MeshRenderer、Rigidbody 等常用组件
   - 没有成熟的 Component 注册/查询机制，所有 gameObject 通过 Map 遍历

2. **植被系统缺少 LOD**
   - 3000 个实例均使用相同复杂度的广告牌，远端树木无简化，GPU 负载恒定
   - 缺少距离分级或层次化实例缓冲

3. **阴影系统只有单一方向光**
   - 仅支持一个方向光的正交投影阴影，不支持点光源阴影（Omnidirectional Shadow Map）
   - 阴影贴图分辨率固定为 2048x2048，无可配置选项

4. **缺少遮挡剔除**
   - 已实现视锥体剔除，但无遮挡剔除（Occlusion Culling），被前方物体遮挡的物体仍会提交绘制

5. **错误处理可进一步增强**
   - 部分仍使用 `assert()`，可统一为 `VK_CHECK` 宏 + 适当的错误日志
   - 缺少 Vulkan Validation Layer 的运行时开关

6. **材质系统功能有限**
   - 不支持材质实例共享（相同材质参数无法复用纹理绑定）
   - 缺少材质参数持久化（从文件加载材质预设）

7. **平台限制**
   - 当前仅支持 Windows 平台 (部分依赖 Windows API)
   - 着色器路径硬编码为 Windows 格式

---

## 下一步目标

> **方向调整：** 重点转向**图形画面质量优化**，暂不投入时间在引擎架构/基础设施上。优先提升视觉表现力和渲染效果，而非引擎通用性。

### 短期目标 (Short Term)

| 优先级 | 目标 | 说明 |
|--------|------|------|
| **高** | **后处理特效管线** | 添加全屏后处理 Pass：Bloom（高亮溢出泛光）、HDR Tone Mapping（ACES 色调映射）、Gamma 校正，显著提升画面质感 |
| **高** | **抗锯齿 (Anti-Aliasing)** | 集成 MSAA（硬件多重采样）或 FXAA/SMAA（后处理抗锯齿），消除几何体边缘锯齿，提升画面干净度 |
| **中** | **阴影系统增强** | 支持可配置的阴影贴图分辨率（1024/2048/4096）；添加 Percentage-Closer Filtering (PCF) 软阴影；实验性点光源阴影（Cube Shadow Map） |
| **中** | **PBR 管线完善 (IBL)** | 支持 Image-Based Lighting：漫反射辐照度贴图 + 镜面反射预过滤贴图 + BRDF LUT，让 PBR 材质在环境光下更真实 |
| **中** | **植被 LOD 系统** | 根据摄像机距离自动切换植被复杂度：近处广告牌、远处简化为十字面片；减少远处实例的顶点数 |

### 中期目标 (Mid Term)

| 优先级 | 目标 | 说明 |
|--------|------|------|
| **高** | **屏幕空间环境光遮蔽 (SSAO)** | 基于深度缓冲计算屏幕空间 AO，增强角落/缝隙处的阴影深度，提升场景立体感 |
| **中** | **延迟渲染 (Deferred Rendering)** | 重构为 GBuffer 多 Render Target 管线（Albedo + Normal + Depth + Material），支持大量动态光源的实时渲染 |
| **中** | **粒子系统** | GPU 驱动的粒子系统：火焰、烟雾、落叶、雨滴等；支持粒子生命周期、物理模拟、与风力系统联动 |
| **中** | **体积光 (Volumetric Light)** | 基于 Ray Marching 的屏幕空间体积光效果（God Rays），增强方向光和点光源的大气散射感 |
| **低** | **地形材质增强** | 基于坡度/高度的多层纹理混合（Splat Mapping），法线贴图采样，提升地表细节表现力 |
| **低** | **水面渲染** | 带反射/折射的实时水面：屏幕空间反射 (SSR) + 法线贴图扰动 + 菲涅尔效应 + 岸边泡沫 |

### 长期目标 (Long Term)

| 优先级 | 目标 | 说明 |
|--------|------|------|
| **中** | **屏幕空间反射 (SSR)** | 基于深度缓冲的屏幕空间反射，为水面、金属表面等提供实时反射效果 |
| **低** | **级联阴影映射 (CSM)** | 多级联方向光阴影，解决大场景阴影精度不足的问题，近处高精度远处低精度 |
| **低** | **骨骼动画** | 支持蒙皮网格（Skinned Mesh）+ 动画状态机；为角色和动态物体提供动画能力 |
| **低** | **景深 (Depth of Field)** | 基于物理的 Bokeh 景深效果，模拟摄像机焦点外的模糊 |
| **低** | **运动模糊 (Motion Blur)** | 基于速度缓冲的逐像素运动模糊，增强动态场景的速度感 |
| **低** | **HDR 天空 & 大气散射** | 程序化天空：Rayleigh/Mie 大气散射 + 昼夜循环 + 体积云，替代静态天空盒立方体贴图 |

---

## 依赖

| 依赖 | 版本 | 用途 |
|------|------|------|
| **Vulkan SDK** | >= 1.3 | 图形 API |
| **GLFW** | 3.x (预编译) | 窗口创建 + 输入处理 |
| **GLM** | 任意 | 数学库 (头文件) |
| **ImGui** | 源码集成 | 调试 UI |
| **tinyobjloader** | 源码集成 | OBJ 模型加载 |
| **stb_image** | 源码集成 | 纹理图像加载 |
| **CMake** | >= 3.20 | 构建系统 |

---

*此文档记录了 Vulkan-Learning 项目的当前状态（截至 2026 年 5 月），将随项目演进持续更新。*
