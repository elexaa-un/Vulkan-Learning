// Vulkan学习项目 — 全屏后处理系统
// 将场景渲染到离屏纹理，再通过全屏三角形进行后处理效果输出

#pragma once

#include "../lve_pipeline.hpp"
#include "../lve_device.hpp"
#include "../lve_gameobject.hpp"
#include "../lve_camera.hpp"
#include "../lve_frame_info.hpp"
#include "../lve_descriptors.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <memory>
#include <vector>

namespace lve
{
    // ============================================================
    // 全屏后处理系统
    // ============================================================
    // 作用：
    //   1. 创建一张离屏颜色纹理，场景先渲染到这张纹理上
    //   2. 使用全屏三角形将离屏纹理采样、后处理、输出到 SwapChain
    //
    // 整体渲染流程：
    //   [旧流程] 场景 → SwapChain 图像（直接显示）
    //   [新流程] 场景 → 离屏颜色纹理 → 后处理 Pass → SwapChain 图像
    //
    // 这个设计是"所有项目都可以实现"的通用方案：
    //   - 只需要一个独立的 RenderPass + 一张颜色纹理
    //   - 不需要修改场景渲染管线
    //   - 后处理效果完全在片段着色器中完成
    // ============================================================
    class LvePostProcessingSystem
    {
    public:
        // -------- 构造函数 --------
        LvePostProcessingSystem(LveDevice &device,
                                VkFormat swapChainFormat,
                                VkExtent2D extent,
                                VkRenderPass outputRenderPass);
        ~LvePostProcessingSystem();

        LvePostProcessingSystem(const LvePostProcessingSystem &) = delete;
        LvePostProcessingSystem &operator=(const LvePostProcessingSystem &) = delete;

        // ========== 公共接口 ==========

        VkRenderPass getOffscreenRenderPass() const { return m_offscreenRenderPass; }
        VkFramebuffer getOffscreenFramebuffer() const { return m_offscreenFramebuffer; }
        VkImageView getOffscreenImageView() const { return m_offscreenImageView; }
        VkSampler getOffscreenSampler() const { return m_offscreenSampler; }

        // Descriptor set layout（后处理着色器用）
        VkDescriptorSetLayout getDescriptorSetLayout() const;

        // 后处理 descriptor set（每帧绑定到后处理管线）
        VkDescriptorSet getDescriptorSet() const;

        // 离屏 RenderPass 的开始和结束
        void beginOffscreenRenderPass(VkCommandBuffer commandBuffer) const;
        void endOffscreenRenderPass(VkCommandBuffer commandBuffer) const;

        // 执行后处理渲染
        void render(FrameInfo &frameInfo);

        // 重新创建离屏资源（窗口大小改变时调用）
        void recreate(VkFormat swapChainFormat, VkExtent2D extent, VkRenderPass outputRenderPass);

    private:
        // ========== 初始化步骤 ==========
        void createOffscreenImage(VkFormat format, VkExtent2D extent);
        void createOffscreenImageView(VkFormat format);
        void createOffscreenSampler();
        void createOffscreenDepthImage(VkExtent2D extent);
        void createOffscreenDepthImageView();
        void createOffscreenRenderPass(VkFormat format);
        void createOffscreenFramebuffer(VkExtent2D extent);
        void createDescriptorSetLayout();
        void createDescriptorPoolAndSet();
        void createPipelineLayout();
        void createPipeline(VkRenderPass outputRenderPass);

        void cleanupResources();

        // ========== 成员变量 ==========
        LveDevice &lveDevice;

        // 离屏颜色纹理
        VkFormat m_offscreenFormat;                         // 离屏纹理实际使用的格式（线性 HDR）
        VkImage m_offscreenImage = VK_NULL_HANDLE;
        VkDeviceMemory m_offscreenMemory = VK_NULL_HANDLE;
        VkImageView m_offscreenImageView = VK_NULL_HANDLE;
        VkSampler m_offscreenSampler = VK_NULL_HANDLE;

        // 离屏深度纹理（Framebuffer 需要的深度附件）
        VkImage m_offscreenDepthImage = VK_NULL_HANDLE;
        VkDeviceMemory m_offscreenDepthMemory = VK_NULL_HANDLE;
        VkImageView m_offscreenDepthImageView = VK_NULL_HANDLE;
        VkFormat m_offscreenDepthFormat;

        // 离屏 RenderPass + Framebuffer
        VkRenderPass m_offscreenRenderPass = VK_NULL_HANDLE;
        VkFramebuffer m_offscreenFramebuffer = VK_NULL_HANDLE;

        // Descriptor
        std::unique_ptr<LveDescriptorSetLayout> m_descriptorSetLayout;
        std::unique_ptr<LveDescriptorPool> m_descriptorPool;
        VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

        // 后处理管线
        std::unique_ptr<LvePipeline> m_pipeline;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

        // 当前格式/尺寸（用于重建）
        VkFormat m_currentFormat;
        VkExtent2D m_currentExtent;
    };
}
