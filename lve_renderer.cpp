#include "lve_renderer.hpp"
#include "lve_utils.hpp"
#include <stdexcept>
#include <array>
#include <iostream>
namespace lve
{
    LveRenderer::LveRenderer(LveWindow &window, LveDevice &device) : lveWindow{window}, lveDevice{device}
    {
        recreateSwapChain();
        // 注意：recreateSwapChain() 内部已调用 createCommandBuffers()，无需重复调用
    }
    LveRenderer::~LveRenderer()
    {
        FreeCommandBuffers();
    }
    void LveRenderer::recreateSwapChain()
    {
        auto extent = lveWindow.getExtent();
        while (extent.width == 0 || extent.height == 0)
        {
            extent = lveWindow.getExtent();
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(lveDevice.device());
        FreeCommandBuffers();

        if (lveSwapChain == nullptr)
        {
            lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent);
        }
        else
        {
            // 【关键】在移动之前保存格式信息
            VkFormat oldImageFormat = lveSwapChain->getSwapChainImageFormat();
            VkFormat oldDepthFormat = lveSwapChain->findDepthFormat(); // 或者添加 getter

            std::shared_ptr<LveSwapChain> oldSwapChain = std::move(lveSwapChain);
            lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent, std::move(oldSwapChain));

            // 使用保存的值进行比较，而不是访问已销毁的 oldSwapChain
            if (oldImageFormat != lveSwapChain->getSwapChainImageFormat() ||
                oldDepthFormat != lveSwapChain->getSwapChainDepthFormat())
            {
                std::cout << "Warning: Swap chain formats changed!" << std::endl;
                // 设置标志，让外部重建 Pipeline
            }
        }
        currentFrameIndex = 0;
        currentImageIndex = 0;
        createCommandBuffers();
    }

    void LveRenderer::createCommandBuffers()
    {
        // 确保先释放已有的命令缓冲区
        if (!commandBuffers.empty())
        {
            std::cout << "Warning: Creating command buffers but existing ones not freed first" << std::endl;
            FreeCommandBuffers();
        }

        commandBuffers.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);

        std::cout << "Allocating " << commandBuffers.size() << " command buffers" << std::endl;

        VkCommandBufferAllocateInfo alloInfo{};
        alloInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloInfo.commandPool = lveDevice.getCommandPool();
        alloInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        VK_CHECK(vkAllocateCommandBuffers(lveDevice.device(), &alloInfo, commandBuffers.data()));

        std::cout << "Successfully allocated " << commandBuffers.size() << " command buffers" << std::endl;
    }
    void LveRenderer::FreeCommandBuffers()
    {
        // 如果没有命令缓冲区需要释放，直接返回
        if (commandBuffers.empty())
        {
            std::cout << "No command buffers to free" << std::endl;
            return;
        }

        std::cout << "Freeing " << commandBuffers.size() << " command buffers" << std::endl;

        vkFreeCommandBuffers(lveDevice.device(),
                             lveDevice.getCommandPool(),
                             static_cast<uint32_t>(commandBuffers.size()),
                             commandBuffers.data());
        commandBuffers.clear();
    }
    VkCommandBuffer LveRenderer::beginFrame()
    {
        // Guard against re-entry (should not happen in normal flow)
        if (isFrameStarted)
        {
            std::cerr << "FATAL: beginFrame called while already in progress! Aborting current frame." << std::endl;
            std::cerr << "  currentFrameIndex: " << currentFrameIndex << std::endl;
            std::cerr << "  commandBuffers size: " << commandBuffers.size() << std::endl;
            std::cerr << "  lveSwapChain valid: " << (lveSwapChain != nullptr) << std::endl;
            isFrameStarted = false;
        }

        if (currentFrameIndex >= static_cast<int>(commandBuffers.size()))
        {
            currentFrameIndex = 0;
        }

        auto result = lveSwapChain->acquireNextImage(&currentImageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapChain();
            // isFrameStarted is guaranteed false after recreateSwapChain() + the guard above,
            // but be explicit for safety
            isFrameStarted = false;
            return nullptr;
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("failed to acquire swap chain image");
        }

        isFrameStarted = true;
        auto commandBuffer = getCurrentCommandBuffer();
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        return commandBuffer;
    }
    void LveRenderer::endFrame()
    {
        LVE_ASSERT(isFrameStarted, "Cannot call endFrame while frame not in progress");
        auto commandBuffer = getCurrentCommandBuffer();
        VK_CHECK(vkEndCommandBuffer(commandBuffer));
        auto result = lveSwapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || lveWindow.wasWindowResized())
        {
            lveWindow.resetWindowResizedFlag();
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS)
        {
            throw std::runtime_error("failed to present swap chain image");
        }

        isFrameStarted = false;
        currentFrameIndex = (currentFrameIndex + 1) % LveSwapChain::MAX_FRAMES_IN_FLIGHT;
        // Implementation for ending the current frame
    }
    void LveRenderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer)
    {
        LVE_ASSERT(isFrameStarted, "Cannot begin render pass when frame not in progress");
        LVE_ASSERT(commandBuffer == getCurrentCommandBuffer(), "Cannot begin render pass on a command buffer from a different frame");
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = lveSwapChain->getRenderPass();
        renderPassInfo.framebuffer = lveSwapChain->getFrameBuffer(currentImageIndex);

        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = lveSwapChain->getSwapChainExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {0.01f, 0.01f, 0.01f, 0.01f};
        clearValues[1].depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = 2;
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(lveSwapChain->width());
        viewport.height = static_cast<float>(lveSwapChain->height());
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{{0, 0}, lveSwapChain->getSwapChainExtent()};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }
    void LveRenderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer)
    {
        LVE_ASSERT(isFrameStarted, "Cannot end render pass when frame not in progress");
        LVE_ASSERT(commandBuffer == getCurrentCommandBuffer(), "Cannot end render pass on a command buffer from a different frame");
        vkCmdEndRenderPass(commandBuffer);
    }
}
