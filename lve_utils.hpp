#pragma once
#include <stdexcept>
#include <string>
#include <iostream>
// 宏1：VK_CHECK —— 检查 Vulkan API 调用的 VkResult 返回值
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
    template <typename T, typename... Rest>
    void hash_combine(std::size_t &seed, const T &v, const Rest &...rest)
    {
        seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        (hash_combine(seed, rest), ...);
    };

} // namespace lve
