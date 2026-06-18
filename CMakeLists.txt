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
# 着色器编译 —— 自动将 .vert/.frag 编译为 SPIR-V (.spv)
# 只有修改过的着色器才会重新编译（增量构建）
# ============================================================
# 定位 glslc 编译器
find_program(GLSLC glslc
    PATHS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VK_SDK_PATH}/Bin"
    DOC "Vulkan GLSL-to-SPIR-V compiler (glslc)"
)
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found! "
                        "Please install Vulkan SDK or set VULKAN_SDK environment variable.")
endif()
message(STATUS "Found glslc: ${GLSLC}")

# 收集所有着色器源文件（包括计算着色器 .comp）
file(GLOB SHADER_SOURCES
    "shaders/*.vert"
    "shaders/*.frag"
    "shaders/*.comp"
)

# 输出目录
set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# 为每个着色器源文件生成 .spv 编译规则
# 注意：file(GLOB) 返回的是绝对路径，直接使用即可，不要再拼 CMAKE_SOURCE_DIR
set(SPV_FILES "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV_FILE "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPV_FILE}
        COMMAND ${GLSLC} "${SHADER}" -o "${SPV_FILE}"
        DEPENDS "${SHADER}"
        COMMENT "Compiling shader: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
    )
    list(APPEND SPV_FILES ${SPV_FILE})
endforeach()

# 汇总所有 .spv 文件作为一个构建目标
add_custom_target(CompileShaders ALL
    DEPENDS ${SPV_FILES}
    COMMENT "All shaders compiled to SPIR-V"
)

# 让主程序依赖着色器编译（构建 exe 前先编译着色器）
add_dependencies(Vulkan_Learning CompileShaders)

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

add_custom_command(TARGET Vulkan_Learning POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/rain.png
    $<TARGET_FILE_DIR:Vulkan_Learning>/rain.png
    COMMENT "Copying rain particle texture")

add_custom_command(TARGET Vulkan_Learning POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/snow.png
    $<TARGET_FILE_DIR:Vulkan_Learning>/snow.png
    COMMENT "Copying snow particle texture")

# 复制 glfw3.dll 到 build 目录（exe 运行时需要）
add_custom_command(TARGET Vulkan_Learning POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/glfw/lib/glfw3.dll
    $<TARGET_FILE_DIR:Vulkan_Learning>/glfw3.dll
    COMMENT "Copying glfw3.dll to build/")
