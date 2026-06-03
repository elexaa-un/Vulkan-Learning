// Vulkan学习项目 — 渲染器
// 管理交换链、命令缓冲区和帧渲染生命周期的顶层协调器

#pragma once

#include "lve_window.hpp"
#include "lve_device.hpp"
#include "lve_swap_chain.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <memory>
#include <cassert>
#include <vector>
namespace lve
{
    // 渲染器：封装每帧 beginFrame → 渲染 → endFrame 的完整流程
    class LveRenderer
    {
    public:
        LveRenderer(LveWindow &window, LveDevice &device);
        ~LveRenderer();

        LveRenderer(const LveRenderer &) = delete;
        LveRenderer &operator=(const LveRenderer &) = delete;

        // 获取交换链渲染通道、宽高比、图像格式和范围
        VkRenderPass GetSwapChainRenderPass() const { return lveSwapChain->getRenderPass(); }
        float getAspectRatio() const { return lveSwapChain->extentAspectRatio(); }
        VkFormat getSwapChainImageFormat() const { return lveSwapChain->getSwapChainImageFormat(); }
        VkExtent2D getSwapChainExtent() const { return lveSwapChain->getSwapChainExtent(); }

        // 开始一帧渲染
        // 执行步骤：
        //   1. 等待上一帧完成（WaitForFences）
        //   2. 获取下一张交换链图像（AcquireNextImage）
        //   3. 重置并开始记录命令缓冲区
        VkCommandBuffer beginFrame();

        // 结束一帧渲染
        // 执行步骤：
        //   1. 结束命令缓冲区记录
        //   2. 提交命令缓冲区到图形队列
        //   3. 呈现交换链图像（Present）
        void endFrame();

        // 开始/结束交换链渲染通道（vkCmdBeginRenderPass / vkCmdEndRenderPass）
        void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
        void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

        bool isFrameInProgress() const { return isFrameStarted; }
        VkCommandBuffer getCurrentCommandBuffer() const
        {
            assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
            return commandBuffers[currentFrameIndex];
        }
        int getFrameIndex() const
        {
            assert(isFrameStarted && "Cannot get frame index when frame not in progress");
            if (currentFrameIndex >= static_cast<int>(commandBuffers.size()))
            {
                return 0;
            }
            return currentFrameIndex;
        }

        // 检查/重置格式变化标志（窗口大小变化导致交换链重建）
        bool hasFormatsChanged() const { return formatsChanged; }
        void resetFormatsChangedFlag() { formatsChanged = false; }

    private:
        // 创建命令缓冲区（从设备命令池分配）
        void createCommandBuffers();
        // 重建交换链（处理窗口大小变化）
        void recreateSwapChain();
        // 释放命令缓冲区
        void FreeCommandBuffers();

        LveWindow &lveWindow;
        LveDevice &lveDevice;
        std::unique_ptr<LveSwapChain> lveSwapChain;  // 交换链
        std::vector<VkCommandBuffer> commandBuffers;  // 命令缓冲区（飞行帧数量）
        uint32_t currentImageIndex;                   // 当前交换链图像索引
        int currentFrameIndex = 0;                    // 当前飞行帧索引
        bool isFrameStarted;                          // 帧是否已开始
        bool formatsChanged = false;                  // 交换链格式是否已变化
    };
}
