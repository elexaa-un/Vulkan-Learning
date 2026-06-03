// Vulkan学习项目 — 纹理管理器（Fallback资源）
// 提供全局静态回退纹理资源：默认采样器和1x1白色纹理

#pragma once
#include "lve_device.hpp"
#include <string>
#include <memory>

namespace lve
{
    // 纹理回退资源管理器：在纹理加载失败或未加载时提供安全的默认资源
    // 所有方法均为静态方法，确保全局唯一
    class LveTextureManager
    {
    public:
        // 初始化回退资源：创建1x1白色纹理图像视图和默认采样器
        // 必须在Vulkan设备创建后调用一次
        // 执行步骤：
        //   1. createDefaultSampler — 创建线性过滤+重复寻址的默认采样器
        //   2. createWhiteImageView — 创建1x1白色RGBA8图像和图像视图
        static void initFallbackResources(VkDevice device, VkPhysicalDevice physicalDevice)
        {
            s_fallbackSampler = createDefaultSampler(device, physicalDevice);
            s_fallbackImageView = createWhiteImageView(device, physicalDevice);
        }

        // 清理所有回退资源（在cleanup时调用）
        static void cleanupFallbackResources(VkDevice device);

        static VkSampler getFallbackSampler() { return s_fallbackSampler; }
        static VkImageView getFallbackImageView() { return s_fallbackImageView; }

    private:
        // 创建最小默认采样器（线性过滤，重复寻址模式）
        static VkSampler createDefaultSampler(VkDevice device, VkPhysicalDevice physicalDevice);

        // 创建1x1白色RGBA8图像并返回其图像视图
        static VkImageView createWhiteImageView(VkDevice device, VkPhysicalDevice physicalDevice);

        static VkSampler s_fallbackSampler;           // 回退采样器
        static VkImageView s_fallbackImageView;       // 回退图像视图
        static VkImage s_fallbackImage;               // 回退图像
        static VkDeviceMemory s_fallbackImageMemory;  // 回退图像内存
    };
}
