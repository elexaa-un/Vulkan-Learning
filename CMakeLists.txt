cmake_minimum_required(VERSION 3.20)

project(MyProject VERSION 1.0 LANGUAGES CXX)

# 不要递归查找，或者明确排除 build 目录
# 方案1：只查找当前目录和 systems 目录（推荐）
file(GLOB SOURCES "*.cpp" "systems/*.cpp" "ImGui/*.cpp")
file(GLOB HEADERS "*.hpp" "*.h" "systems/*.hpp" "systems/*.h")

# 方案2：如果坚持用 GLOB_RECURSE，排除 build 目录
# file(GLOB_RECURSE SOURCES LIST_DIRECTORIES false "*.cpp" "!build/*")
# file(GLOB_RECURSE HEADERS LIST_DIRECTORIES false "*.hpp" "*.h" "!build/*")

# 创建可执行文件（只添加源文件）
add_executable(Vulkan_Learning ${SOURCES})

# 使用 C++17
target_compile_features(Vulkan_Learning PRIVATE cxx_std_17)

# 包含目录（注意：路径用空格分隔，不是逗号）
target_include_directories(Vulkan_Learning PUBLIC 
    "glfw/include"
    "."              # 添加项目根目录
    "systems"        # 添加 systems 目录
    "ImGui"          # 添加 ImGui 目录
)

# 库目录
target_link_directories(Vulkan_Learning PUBLIC "glfw/lib")

# Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(Vulkan_Learning PRIVATE Vulkan::Vulkan)

# 链接 GLFW
target_link_libraries(Vulkan_Learning PRIVATE glfw3 dwmapi)

# ============================================================
# 自动复制资源文件到 build 目录（exe 所在目录）
# 这样 exe 用相对路径就能找到贴图、模型、天空盒等
# ============================================================
add_custom_command(TARGET Vulkan_Learning POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/skybox
    $<TARGET_FILE_DIR:Vulkan_Learning>/skybox
    COMMENT "Copying skybox textures to build/")

add_custom_command(TARGET Vulkan_Learning POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/apple
    $<TARGET_FILE_DIR:Vulkan_Learning>/apple
    COMMENT "Copying apple model to build/")

add_custom_command(TARGET Vulkan_Learning POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/tree.png
    $<TARGET_FILE_DIR:Vulkan_Learning>/tree.png
    COMMENT "Copying tree texture")

add_custom_command(TARGET Vulkan_Learning POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/tt.jpg
    $<TARGET_FILE_DIR:Vulkan_Learning>/tt.jpg
    COMMENT "Copying terrain texture")
