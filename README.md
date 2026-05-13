# Vulkan-Learning

> 一个基于 Vulkan API 的实时 3D 渲染引擎学习项目，支持 PBR 材质、动态阴影、植被风力系统、天空盒、ImGui 调试面板等功能。

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

**Vulkan-Learning** 是一个从零开始构建的 Vulkan 实时渲染引擎，旨在学习 Vulkan API 的底层运作机制。项目采用 C++17 编写，使用 CMake 构建系统，渲染核心自研，仅依赖于 GLFW（窗口）、Vulkan SDK 和 ImGui（调试界面）。项目目前支持完整的 3D 场景渲染，包含地形、模型、植被、天空盒、动态光源和实时阴影。

---

## 核心特性

| 特性 | 说明 |
|------|------|
| **Vulkan 底层封装** | 自研 `lve_*` 系列类，封装设备、交换链、管线、缓冲区、纹理、描述符等核心概念 |
| **PBR 材质系统** | 支持基础色、法线、高光贴图，可配置金属度/粗糙度 |
| **地形系统** | 基于高度图自动生成地形网格 + 纹理采样 |
| **动态点光源** | 最多支持 10 个点光源，可实时调节位置/强度/颜色 |
| **Shadow Mapping** | 2048×2048 深度阴影贴图，支持实时阴影投射 |
| **天空盒** | 立方体贴图天空盒渲染 |
| **植被系统** | GPU 实例化渲染（1000+ 实例）+ 广告牌 + 程序化风力动画 |
| **ImGui 调试面板** | 实时显示 FPS、摄像机参数、光源调节、风力强度、渲染选项 |
| **自由摄像机** | WASD 移动 + 鼠标旋转视角 |

---

## 项目架构

```
┌─────────────────────────────────────────────────────────┐
│                      main.cpp                            │
│                    (程序入口)                              │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────┐
│                    FirstApp (主控类)                       │
│  ? 持有所有引擎组件                                         │
│  ? 管理渲染循环 (run)                                      │
│  ? 加载场景 (LoadGameObjects)                              │
│  ? 协调各渲染系统                                           │
└───────┬─────────────────────────────────────────────────┘
        │
        │ 持有/创建
        ├──→ LveWindow          (GLFW 窗口)
        ├──→ LveDevice          (Vulkan 设备)
        ├──→ LveRenderer        (帧渲染控制)
        │     └──→ LveSwapChain (交换链 + RenderPass)
        ├──→ LveCamera          (摄像机)
        ├──→ LveImgui           (ImGui 集成)
        │
        │ 渲染系统 (systems/)
        ├──→ LveSimpleRenderSystem    (PBR 模型渲染)
        ├──→ LvePointLightSystem      (点光源渲染)
        ├──→ LveSkyboxSystem          (天空盒渲染)
        ├──→ LveShadowSystem          (阴影贴图生成)
        ├──→ LveVegetationSystem      (植被实例化渲染)
        │
        │ 核心引擎 (lve_*)
        ├──→ LveModel           (模型加载/顶点缓冲区)
        ├──→ LveTexture         (纹理/立方体贴图)
        ├──→ LveMaterial        (材质管理)
        ├──→ LveGameObject      (游戏对象 + ECS 雏形)
        ├──→ LvePipeline        (图形管线封装)
        ├──→ LveBuffer          (VBO/UBO 管理)
        ├──→ LveDescriptors     (描述符集布局/池/写入)
        └──→ LveFrameInfo       (帧数据: UBO + FrameInfo)
```

### 渲染管线流程

```
每帧执行顺序:

1. beginFrame()                  → 获取 CommandBuffer + ImageIndex
2. Shadow Pass                   → 从光源视角渲染深度贴图
3. beginSwapChainRenderPass()    → 开始主渲染通道
4. SkyboxSystem.render()         → 绘制天空盒
5. SimpleRenderSystem.render()   → 绘制 PBR 模型 (apple + terrain)
6. VegetationSystem.render()     → 绘制植被实例 (1000 trees)
7. PointLightSystem.render()     → 绘制光源可视化球
8. endSwapChainRenderPass()      → 结束渲染通道
9. ImGui.render()                → 绘制调试面板
10. endFrame()                   → Present + 提交
```

### 描述符集布局

```
Set = 0 (Global Descriptor Set):
  binding = 0: GlobalUbo          (Uniform Buffer)
  binding = 1: Cubemap Sampler    (天空盒纹理)
  binding = 2: Shadow Map Sampler (阴影贴图)

Set = 1 (Material Descriptor Set):
  binding = 0: Material UBO       (材质参数)
  binding = 1: Diffuse Texture    (漫反射贴图)
  binding = 2: Normal Texture     (法线贴图)
  binding = 3: Specular Texture   (高光贴图)

Set = 2 (Vegetation Texture Set):
  binding = 0: Tree Texture       (植物贴图)
```

---

## 模块详解

### 引擎层 (Engine Core)

#### `LveWindow` — 窗口管理
- 基于 GLFW 封装窗口创建、事件轮询
- 回调处理窗口大小调整
- 暴露 `VkSurfaceKHR` 创建接口

#### `LveDevice` — Vulkan 设备封装
- 物理设备选择（GPU 选择逻辑）
- 逻辑设备创建 + 队列族选择（Graphics + Present）
- 命令池管理
- 设备属性查询

#### `LveSwapChain` — 交换链
- 三重缓冲交换链创建/重建
- 深度缓冲区管理
- RenderPass 封装
- 窗口大小变化自动重建

#### `LveRenderer` — 帧渲染控制
- 单帧生命周期管理 (beginFrame/endFrame)
- CommandBuffer 分配与回收
- 多帧同步 (Fence/Semaphore)
- 交换链重建触发

#### `LvePipeline` — 图形管线
- Shader 模块加载 (SPIR-V)
- 管线状态配置 (深度测试、混合、背面剔除等)
- 支持可配置的顶点输入 + 实例化

#### `LveBuffer` — 缓冲区管理
- VBO / IBO / UBO 统一封装
- Staging Buffer 模式（设备本地内存）
- Host-Visible 内存映射
- 对齐约束处理

#### `LveTexture` — 纹理管理
- 2D 纹理加载 (stb_image)
- Mipmap 生成
- 立方体贴图创建（天空盒）
- 采样器配置

#### `LveDescriptors` — 描述符系统
- **Builder 模式**: `LveDescriptorSetLayout` / `LveDescriptorPool` 均提供 Builder
- **Writer 模式**: `LveDescriptorWriter` 链式写入 + 一次性构建
- 支持增量覆盖 (overwrite)

#### `LveModel` — 模型系统
- OBJ 文件加载 (tinyobjloader)
- 顶点格式: Position + Color + Normal + UV
- Builder 模式创建
- 静态几何体生成 (天空盒、地形)

#### `LveGameObject` — 游戏对象
- ECS 雏形：Transform + PointLight 组件
- `std::unordered_map<id, GameObject>` 管理
- 关联 Model / Material / Texture

#### `LveMaterial` — 材质系统
- PBR 材质参数: BaseColor/Metallic/Roughness
- 三张贴图槽 (Diffuse / Normal / Specular)
- 材质管理器 (`LveMaterialManager`) 工厂模式

#### `LveCamera` — 摄像机
- 透视投影 (可配置 FOV / Aspect / Near / Far)
- 基于 YXZ Tait-Bryan 角度的旋转
- View / Projection / InverseView 矩阵

#### `LveFrameInfo` — 帧数据结构
- `FrameInfo`: 帧元数据 (index, time, commandBuffer, camera, descriptorSet, gameObjects)
- `GlobalUbo`: 全局 UBO (projection, view, lightViewProj, pointLights, wind params)

---

### 渲染系统层 (Render Systems)

#### `LveSimpleRenderSystem` — 主渲染系统
- 使用 `simple_shader` (vert + frag)
- 支持 PBR 材质的两级描述符集 (Global + Material)
- 遍历 gameObjects 渲染所有模型

#### `LvePointLightSystem` — 点光源系统
- 使用 `point_light` shader
- 将点光源渲染为发光球体
- 每帧从 gameObjects 收集光源数据写入 GlobalUbo

#### `LveSkyboxSystem` — 天空盒系统
- 使用 `skybox` shader
- 渲染单位立方体 + cubemap 采样
- 深度测试策略：`GREATER_OR_EQUAL` (始终在场景后方)

#### `LveShadowSystem` — 阴影系统
- 独立的 Shadow RenderPass (只写深度)
- 2048×2048 深度贴图
- 正交投影 (从光源方向)
- 在主着色器中采样 Shadow Map

#### `LveVegetationSystem` — 植被系统
- 使用 `vegetation` shader (广告牌 + 实例化)
- GPU 实例化渲染 (per-instance: position + scale)
- 顶点着色器风力动画 (多频率正弦波叠加)
- 支持 ImGui 实时调节风力参数

---

### 着色器 (Shaders)

| 着色器 | 用途 | Vertex | Fragment |
|--------|------|--------|----------|
| `simple_shader` | PBR 模型渲染 | ? | ? |
| `point_light` | 点光源可视化 | ? | ? |
| `skybox` | 天空盒渲染 | ? | ? |
| `shadow` | 阴影深度写入 | ? | ? (空) |
| `vegetation` | 植被广告牌+风动 | ? | ? |

---

## 构建与运行

### 前置依赖

- **Vulkan SDK** (≥ 1.3): [LunarG Vulkan SDK](https://vulkan.lunarg.com/)
- **GLFW**: 项目中已包含预编译库 (`glfw/lib/`)
- **GLM**: 头文件库 (通过 CMake 查找或 vcpkg 安装)
- **CMake** (≥ 3.20)
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

# 4. 编译
cmake --build .

# 5. 运行
./Vulkan_Learning.exe
```

> 项目已配置 POST_BUILD 自动复制资源文件（skybox/、apple/、tree.png、tt.jpg）到构建目录。

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
| 移动摄像机 | W / A / S / D |
| 旋转视角 | 鼠标右键拖拽 |
| 缩放视野 | 滚轮 |
| 退出程序 | ESC |
| 调出调试面板 | 自动显示 |
| 调节风力强度 | ImGui → Vegetation → Wind Strength |
| 调节光源参数 | ImGui → Lighting |

---

## 项目优缺点分析

### 优点

1. **Vulkan 概念覆盖全面**
   - 项目覆盖了 Vulkan 核心概念：设备管理、交换链、管线、描述符、缓冲区、同步等，是对 Vulkan 学习的良好实践

2. **模块化引擎设计**
   - `lve_*` 前缀的引擎模块职责清晰：Device、Window、Renderer、Pipeline、Buffer、Texture 等各司其职
   - Builder 模式广泛用于构造复杂对象（DescriptorSetLayout、DescriptorPool、Model）

3. **多渲染系统并存**
   - 5 个独立的渲染系统共享全局 UBO 和描述符集，展示了多 Pass 渲染的架构能力
   - Shadow Mapping 的独立 RenderPass + Framebuffer 设计合理

4. **现代化图形特性**
   - PBR 材质管线（BaseColor / Metallic / Roughness）
   - GPU 实例化植被渲染（支持 1000+ 实例）
   - 程序化风力动画（顶点着色器计算）
   - 立方体贴图天空盒

5. **实时调试能力**
   - ImGui 集成提供丰富的运行时参数调节：FPS 显示、光源控制、风力调节、摄像机重置

6. **良好的资源管理**
   - CMake POST_BUILD 自动复制资源，开发体验友好
   - 使用 `std::unique_ptr` / `std::shared_ptr` 管理 Vulkan 资源生命周期

### 缺点

1. **主控类过于臃肿**
   - `first_app.cpp` 的 `run()` 函数承担了过多职责：初始化所有渲染系统、创建 UBO、处理 ImGui 逻辑、填充 UBO、协调渲染顺序。单一函数超过 250 行，难以维护和扩展
   - 应引入 **场景管理类** 或 **应用状态机** 解耦

2. **GlobalUbo 结构不合理**
   - 风力参数（`windTime`、`windStrength`、`windSpeed`）与光照/投影矩阵混在同一 UBO 中，语义不清
   - 风力参数应独立为独立 UBO 或 Push Constants，避免每帧重写整个 GlobalUbo

3. **缺少资源管理器**
   - 纹理、模型路径硬编码在 `first_app.cpp` 中
   - 缺少统一的资源加载/缓存机制，相同纹理可能被重复加载

4. **着色器编译流程不自动化**
   - `.spv` 文件需要手动使用 `glslc` 编译
   - CMake 构建不包含 Shader 编译步骤，修改着色器后容易忘记重新编译

5. **缺少渲染优化**
   - 无视锥体剔除 (Frustum Culling)，所有对象每帧都提交绘制
   - 无遮挡剔除 (Occlusion Culling)
   - 植被实例没有 LOD 系统，远处树木与近处使用相同复杂度

6. **ECS 系统不完整**
   - `LveGameObject` 仅支持 Transform + PointLight 组件，扩展性有限
   - 没有成熟的 Component 注册/查询机制

7. **材质系统功能有限**
   - 不支持多材质、材质实例共享
   - 缺少材质参数持久化

8. **错误处理薄弱**
   - 部分使用 `assert()` 而非适当的错误返回或异常处理
   - 缺少 Vulkan 调用的 Validation 层统一封装

9. **代码风格不统一**
   - 部分文件使用 `#include "../lve_xxx.hpp"`（相对路径），部分使用 `#include "lve_xxx.hpp"`
   - 命名风格混用：`LvePointLightSystem` vs `LveShadowSystem` (部分类有 `m_` 前缀成员变量，部分没有)

10. **平台限制**
    - 当前仅支持 Windows 平台 (依赖 `dwmapi`)
    - 着色器路径硬编码为 Windows 格式

---

## 下一步目标

### 短期目标 (Short Term)

| 优先级 | 目标 | 说明 |
|--------|------|------|
| ? 高 | **着色器自动化编译** | 在 CMake 中添加 `glslc` 编译命令，使着色器随项目自动编译 |
| ? 高 | **重构 first_app.cpp** | 拆分 `run()` 函数，抽取 `SceneManager` 类管理场景加载和渲染调度 |
| ? 中 | **独立风力 UBO** | 将风力参数从 `GlobalUbo` 中分离为独立 UBO 或 Push Constants |
| ? 中 | **资源管理器** | 实现统一的 `ResourceManager`，管理纹理/模型的加载、缓存和生命周期 |
| ? 中 | **错误处理增强** | 引入 Vulkan 错误检查宏 (`VK_CHECK`)，替换裸 `assert()` |

### 中期目标 (Mid Term)

| 优先级 | 目标 | 说明 |
|--------|------|------|
| ? 中 | **视锥体剔除** | 使用 AABB + 视锥体检测减少不必要的 Draw Call |
| ? 中 | **LOD 系统** | 植被/地形根据距离切换复杂度等级 |
| ? 中 | **完善 ECS 系统** | 支持灵活的组件注册/查询/遍历 (可参考 EnTT 设计) |
| ? 低 | **多光源支持增强** | 点光源 + 方向光 + 聚光灯混合管线 |
| ? 低 | **法线贴图烘焙** | 在地形渲染中引入法线贴图以提升细节表现 |

### 长期目标 (Long Term)

| 优先级 | 目标 | 说明 |
|--------|------|------|
| ? 低 | **延迟渲染 (Deferred Rendering)** | 重构为 GBuffer 多 Render Target 管线，支持大量动态光源 |
| ? 低 | **后处理特效** | Bloom、HDR Tone Mapping、SSAO |
| ? 低 | **骨骼动画** | 支持蒙皮网格 + 动画状态机 |
| ? 低 | **物理集成** | 集成 Bullet/PhysX 进行碰撞检测与刚体模拟 |
| ? 低 | **跨平台支持** | 适配 Linux / macOS (MoltenVK) |
| ? 低 | **场景编辑器** | 基于 ImGui 的可视化场景编辑工具 |

---

## 依赖

| 依赖 | 版本 | 用途 |
|------|------|------|
| **Vulkan SDK** | ≥ 1.3 | 图形 API |
| **GLFW** | 3.x (预编译) | 窗口创建 + 输入处理 |
| **GLM** | 任意 | 数学库 (头文件) |
| **ImGui** | 源码集成 | 调试 UI |
| **tinyobjloader** | 源码集成 | OBJ 模型加载 |
| **stb_image** | 源码集成 | 纹理图像加载 |
| **CMake** | ≥ 3.20 | 构建系统 |

---

*此文档记录了 Vulkan-Learning 项目的当前状态（截至 2026 年 5 月），将随项目演进持续更新。*
