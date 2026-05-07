# Vulkan 开发环境配置指南

本指南将帮助您配置完整的Vulkan开发环境，包括Vulkan SDK、GLM和GLFW。

## 1. Vulkan SDK 安装

### Windows 系统
1. 访问 [LunarG Vulkan SDK](https://vulkan.lunarg.com/)
2. 下载最新版本的Vulkan SDK
3. 运行安装程序，建议使用默认安装路径
4. 安装完成后，Vulkan SDK会自动设置环境变量

### 验证安装
打开命令行，运行：
```bash
vulkaninfo
```
如果显示GPU信息，说明安装成功。

## 2. GLM 库配置

GLM (OpenGL Mathematics) 是一个头文件库，配置非常简单：

### 方法1：使用vcpkg（推荐）
```bash
vcpkg install glm
```

### 方法2：手动下载
1. 访问 [GLM GitHub](https://github.com/g-truc/glm)
2. 下载最新版本或克隆仓库：
```bash
git clone https://github.com/g-truc/glm.git
```

## 3. GLFW 库配置

GLFW是一个用于创建窗口和处理输入的库。

### 方法1：使用vcpkg（推荐）
```bash
vcpkg install glfw3
```

### 方法2：手动编译
1. 访问 [GLFW官网](https://www.glfw.org/)
2. 下载源代码
3. 使用CMake编译：
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## 4. CMake 配置

创建一个CMakeLists.txt文件来管理项目依赖：

```cmake
cmake_minimum_required(VERSION 3.10)
project(VulkanLearning)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Vulkan
find_package(Vulkan REQUIRED)

# 查找GLFW
find_package(glfw3 REQUIRED)

# 如果使用vcpkg，GLM会自动找到
# 如果手动安装GLM，需要指定路径
# set(GLM_INCLUDE_DIR "path/to/glm")

# 包含头文件目录
include_directories(${Vulkan_INCLUDE_DIRS})
include_directories(${GLFW_INCLUDE_DIRS})
# include_directories(${GLM_INCLUDE_DIR})

# 创建可执行文件
add_executable(VulkanApp main.cpp)

# 链接库
target_link_libraries(VulkanApp 
    ${Vulkan_LIBRARIES}
    glfw
    # 如果需要，可以添加其他库
)

# Windows特定设置
if(WIN32)
    target_link_libraries(VulkanApp ${Vulkan_LIBRARIES})
endif()
```

## 5. 环境变量设置

确保以下环境变量已设置：
- `VULKAN_SDK` - 指向Vulkan SDK安装目录
- `PATH` - 包含Vulkan SDK的bin目录

## 6. IDE 配置

### Visual Studio Code
1. 安装C/C++扩展
2. 安装CMake Tools扩展
3. 配置`.vscode/settings.json`：

```json
{
    "cmake.configureOnOpen": true,
    "cmake.generator": "Ninja",
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools"
}
```

### Visual Studio
1. 安装Visual Studio 2019或更新版本
2. 安装"使用C++的桌面开发"工作负载
3. 确保安装了Windows 10 SDK

## 7. 测试配置

创建一个简单的测试程序来验证配置是否正确。

## 常见问题

### 1. 找不到Vulkan库
- 确保VULKAN_SDK环境变量设置正确
- 检查CMake能否找到Vulkan包

### 2. GLFW链接错误
- 确保GLFW正确编译和安装
- 检查CMakeLists.txt中的库链接设置

### 3. GLM头文件找不到
- 确保GLM_INCLUDE_DIR设置正确
- 检查include路径是否正确

## 推荐的开发工具

- **IDE**: Visual Studio Code 或 Visual Studio
- **包管理器**: vcpkg
- **构建系统**: CMake
- **调试工具**: RenderDoc, NVIDIA Nsight Graphics