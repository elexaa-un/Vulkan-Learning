#pragma once
// Vulkan学习项目 — 通用工具宏与辅助函数

#include <stdexcept>
#include <string>
#include <iostream>

// ------------------------------------------------------------
// VK_CHECK 宏：检查 Vulkan API 调用的 VkResult 返回值
// 执行步骤：
//   1. 调用传入的 Vulkan 函数表达式 (x)，将结果保存到 VkResult
//   2. 如果返回值不是 VK_SUCCESS，抛出一个 std::runtime_error 异常
//   3. 异常消息包含源文件名、行号和失败的函数名
// ------------------------------------------------------------
#define VK_CHECK(x)                                                       \
    do                                                                    \
    {                                                                     \
        VkResult result = (x);                                            \
        if (result != VK_SUCCESS)                                         \
        {                                                                 \
            throw std::runtime_error(                                     \
                std::string("Vulkan error at ") + __FILE__ + ":" +        \
                std::to_string(__LINE__) + " → " + #x +                   \
                " returned " + std::to_string(static_cast<int>(result))); \
        }                                                                 \
    } while (0)

// ------------------------------------------------------------
// LVE_ASSERT 宏：自定义断言，仅在 Debug 模式下生效
// 在 Release 模式 (NDEBUG) 下被编译为空操作
// 执行步骤（Debug 模式）:
//   1. 检查条件 (condition) 是否为真
//   2. 如果为假，向 stderr 输出错误消息和源文件位置
//   3. 调用 std::abort() 终止程序
// ------------------------------------------------------------
#ifdef NDEBUG
#define LVE_ASSERT(condition, message) ((void)0)
#else
#define LVE_ASSERT(condition, message)                              \
    do                                                              \
    {                                                               \
        if (!(condition))                                           \
        {                                                           \
            std::cerr << "ASSERT: " << message                      \
                      << " (" << __FILE__ << ":" << __LINE__ << ")" \
                      << std::endl;                                 \
            std::abort();                                           \
        }                                                           \
    } while (0)
#endif

namespace lve
{
    // 哈希组合函数：结合多个哈希值生成一个新的哈希值
    // 使用 boost hash_combine 算法，通过可变参数模板实现
    // 常用于为自定义类型生成 std::hash 特化
    template <typename T, typename... Rest>
    void hash_combine(std::size_t &seed, const T &v, const Rest &...rest)
    {
        seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        (hash_combine(seed, rest), ...);
    };

} // namespace lve
