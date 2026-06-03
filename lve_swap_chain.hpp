// Vulkan学习项目 — 交换链管理
// 管理Vulkan交换链、渲染通道、帧缓冲和同步对象的完整生命周期

#pragma once

#include "lve_device.hpp"

// vulkan headers
#include <vulkan/vulkan.h>

// std lib headers
#include <memory>
#include <string>
#include <vector>

namespace lve
{

    // 交换链：管理VkSwapchainKHR及关联的渲染资源
    // 负责处理窗口大小变化时的交换链重建（通过 oldSwapChain 传递旧交换链）
    class LveSwapChain
    {
    public:
        static constexpr int MAX_FRAMES_IN_FLIGHT = 3; // 最大同时处理的帧数（三重缓冲）

        // 新建交换链
        LveSwapChain(LveDevice &deviceRef, VkExtent2D windowExtent);
        // 重建交换链（窗口大小变化时），可选传入旧交换链以复用资源
        LveSwapChain(
            LveDevice &deviceRef, VkExtent2D windowExtent, std::shared_ptr<LveSwapChain> previous);
        ~LveSwapChain();

        LveSwapChain(const LveSwapChain &) = delete;
        LveSwapChain &operator=(const LveSwapChain &) = delete;

        // 获取指定索引的帧缓冲
        VkFramebuffer getFrameBuffer(int index) { return swapChainFramebuffers[index]; }
        // 获取渲染通道
        VkRenderPass getRenderPass() { return renderPass; }
        // 获取指定索引的图像视图
        VkImageView getImageView(int index) { return swapChainImageViews[index]; }
        size_t imageCount() { return swapChainImages.size(); }
        VkFormat getSwapChainImageFormat() { return swapChainImageFormat; }
        VkExtent2D getSwapChainExtent() { return swapChainExtent; }
        uint32_t width() { return swapChainExtent.width; }
        uint32_t height() { return swapChainExtent.height; }

        // 计算交换链图像的宽高比
        float extentAspectRatio()
        {
            return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
        }
        // 查找支持的深度格式（32位浮点或24位整数）
        VkFormat findDepthFormat();

        // 获取下一帧图像索引（通过信号量同步）
        VkResult acquireNextImage(uint32_t *imageIndex);
        // 提交命令缓冲区并呈现（通过信号量/栅栏同步）
        VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex);

        // 比较两个交换链的图像格式和深度格式是否相同
        bool compareSwapFormats(const LveSwapChain &swapChain) const
        {
            return swapChain.swapChainDepthFormat == swapChainDepthFormat &&
                   swapChain.swapChainImageFormat == swapChainImageFormat;
        }
        VkFormat getSwapChainDepthFormat() const { return swapChainDepthFormat; }
        VkFormat getSwapChainImageFormat() const { return swapChainImageFormat; }

    private:
        // 初始化：依次调用所有创建函数
        void init();
        // 创建VkSwapchainKHR（选择格式、呈现模式、范围）
        void createSwapChain();
        // 为每个交换链图像创建VkImageView
        void createImageViews();
        // 创建深度缓冲图像和图像视图
        void createDepthResources();
        // 创建渲染通道（定义附件的加载/存储操作）
        void createRenderPass();
        // 为每个交换链图像创建帧缓冲（绑定颜色和深度附件）
        void createFramebuffers();
        // 创建同步对象：信号量（imageAvailable/renderFinished）和栅栏（inFlightFences）
        void createSyncObjects();

        // Helper functions
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(
            const std::vector<VkSurfaceFormatKHR> &availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(
            const std::vector<VkPresentModeKHR> &availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

        VkFormat swapChainImageFormat;     // 交换链颜色附件的格式
        VkFormat swapChainDepthFormat;     // 深度附件的格式
        VkExtent2D swapChainExtent;        // 交换链图像尺寸

        std::vector<VkFramebuffer> swapChainFramebuffers; // 每帧的帧缓冲
        VkRenderPass renderPass;           // 渲染通道

        std::vector<VkImage> depthImages;          // 深度图像
        std::vector<VkDeviceMemory> depthImageMemorys; // 深度图像内存
        std::vector<VkImageView> depthImageViews;  // 深度图像视图
        std::vector<VkImage> swapChainImages;      // 交换链图像
        std::vector<VkImageView> swapChainImageViews; // 交换链图像视图

        LveDevice &device;
        VkExtent2D windowExtent;           // 窗口尺寸

        VkSwapchainKHR swapChain;          // Vulkan交换链句柄
        std::shared_ptr<LveSwapChain> oldSwapChain; // 旧交换链（重建时用于资源过渡）

        // 同步对象（每组对应一帧）
        std::vector<VkSemaphore> imageAvailableSemaphores;  // 图像可用信号量
        std::vector<VkSemaphore> renderFinishedSemaphores;  // 渲染完成信号量
        std::vector<VkFence> inFlightFences;                // 飞行帧栅栏
        std::vector<VkFence> imagesInFlight;                // 每帧图像的使用状态栅栏
        size_t currentFrame = 0;           // 当前帧索引（0..MAX_FRAMES_IN_FLIGHT-1）
    };

} // namespace lve
