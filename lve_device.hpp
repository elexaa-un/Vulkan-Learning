// Vulkan学习项目 — Vulkan逻辑设备封装
// 管理Vulkan实例、物理设备、逻辑设备和命令池的创建与查询

#pragma once

#include "lve_window.hpp"

// std lib headers
#include <string>
#include <vector>

namespace lve
{

    // 交换链支持详情：记录物理设备对交换链的能力支持
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;        // 表面能力（最小/最大图像数、尺寸等）
        std::vector<VkSurfaceFormatKHR> formats;      // 支持的表面格式列表
        std::vector<VkPresentModeKHR> presentModes;   // 支持的呈现模式列表
    };

    // 队列族索引：记录图形队列和呈现队列的族索引
    struct QueueFamilyIndices
    {
        uint32_t graphicsFamily;         // 图形队列族索引
        uint32_t presentFamily;          // 呈现队列族索引
        bool graphicsFamilyHasValue = false; // 是否找到图形队列
        bool presentFamilyHasValue = false;  // 是否找到呈现队列
        // 检查是否同时找到了图形和呈现队列
        bool isComplete() { return graphicsFamilyHasValue && presentFamilyHasValue; }
    };

    // Vulkan逻辑设备：封装Vulkan实例和设备的创建流程，提供常用资源操作
    class LveDevice
    {
    public:
#ifdef NDEBUG
        const bool enableValidationLayers = false;   // Release模式下禁用验证层
#else
        const bool enableValidationLayers = true;    // Debug模式下启用验证层
#endif

        // 构造函数
        // 执行步骤：
        //   1. createInstance() — 创建Vulkan实例
        //   2. setupDebugMessenger() — 配置调试回调（Debug模式）
        //   3. createSurface() — 创建窗口表面
        //   4. pickPhysicalDevice() — 选择物理设备
        //   5. createLogicalDevice() — 创建逻辑设备和队列
        //   6. createCommandPool() — 创建命令池
        LveDevice(LveWindow &window);
        ~LveDevice();

        // 禁止拷贝和移动（内部资源唯一且被其他对象引用）
        LveDevice(const LveDevice &) = delete;
        LveDevice &operator=(const LveDevice &) = delete;
        LveDevice(LveDevice &&) = delete;
        LveDevice &operator=(LveDevice &&) = delete;

        // 获取Vulkan资源和队列句柄
        VkCommandPool getCommandPool() { return commandPool; }
        VkDevice device() { return device_; }
        VkSurfaceKHR surface() { return surface_; }
        VkQueue graphicsQueue() { return graphicsQueue_; }
        VkQueue presentQueue() { return presentQueue_; }
        VkPhysicalDevice getPhysicalDevice() { return physicalDevice; }
        VkInstance getInstance() { return instance; }
        uint32_t getGraphicsQueueFamilyIndex() { return findQueueFamilies(physicalDevice).graphicsFamily; }

        // 查询物理设备对交换链的支持能力
        SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupport(physicalDevice); }

        // 查找满足属性要求的内存类型索引
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

        // 查找物理设备的队列族索引
        QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies(physicalDevice); }

        // 在候选格式列表中查找首个满足平铺和特性要求的格式
        VkFormat findSupportedFormat(
            const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

        // --- 缓冲区辅助函数 ---
        // 创建Vulkan缓冲区和关联的设备内存
        void createBuffer(
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties,
            VkBuffer &buffer,
            VkDeviceMemory &bufferMemory);
        // 开始一次性命令记录（创建临时命令缓冲区）
        VkCommandBuffer beginSingleTimeCommands();
        // 结束一次性命令并提交执行
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);
        // 拷贝缓冲区内容
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        // 将缓冲区内容拷贝到Vulkan图像（支持多层纹理）
        void copyBufferToImage(
            VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);

        // 使用VkImageCreateInfo直接创建图像和关联内存
        void createImageWithInfo(
            const VkImageCreateInfo &imageInfo,
            VkMemoryPropertyFlags properties,
            VkImage &image,
            VkDeviceMemory &imageMemory);

        VkPhysicalDeviceProperties properties; // 当前物理设备的属性

    private:
        // 创建Vulkan实例（设置应用信息、验证层、扩展）
        void createInstance();
        // 设置Vulkan调试回调（仅在Debug模式）
        void setupDebugMessenger();
        // 通过GLFW创建窗口表面
        void createSurface();
        // 选择最合适的物理设备（GPU）
        void pickPhysicalDevice();
        // 创建逻辑设备和获取队列
        void createLogicalDevice();
        // 创建图形命令池
        void createCommandPool();

        // --- 内部辅助函数 ---
        bool isDeviceSuitable(VkPhysicalDevice device);
        std::vector<const char *> getRequiredExtensions();
        bool checkValidationLayerSupport();
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
        void hasGflwRequiredInstanceExtensions();
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

        VkInstance instance;                              // Vulkan实例
        VkDebugUtilsMessengerEXT debugMessenger;          // 调试输出句柄
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; // 选中的物理设备
        LveWindow &window;                                // 关联的窗口（用于创建表面）
        VkCommandPool commandPool;                        // 图形命令池

        VkDevice device_;          // 逻辑设备句柄
        VkSurfaceKHR surface_;     // 窗口表面句柄
        VkQueue graphicsQueue_;    // 图形队列
        VkQueue presentQueue_;     // 呈现队列

        const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"}; // 验证层名称
        const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; // 必需设备扩展
    };

} // namespace lve
