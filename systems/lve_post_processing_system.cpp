// Vulkan学习项目 — 后处理系统实现
// 离屏渲染目标创建、全屏三角形后处理效果输出

#include "lve_post_processing_system.hpp"
#include <stdexcept>
#include <iostream>
#include <array>

namespace lve
{
    // ============================================================
    // getDescriptorSetLayout（非 inline，定义在 .cpp 中）
    // ============================================================
    VkDescriptorSetLayout LvePostProcessingSystem::getDescriptorSetLayout() const
    {
        return m_descriptorSetLayout->getDescriptorSetLayout();
    }

    // ============================================================
    // getDescriptorSet（非 inline，定义在 .cpp 中）
    // ============================================================
    VkDescriptorSet LvePostProcessingSystem::getDescriptorSet() const
    {
        return m_descriptorSet;
    }

    // ============================================================
    // 构造函数
    // ============================================================
    LvePostProcessingSystem::LvePostProcessingSystem(LveDevice &device,
                                                      VkFormat swapChainFormat,
                                                      VkExtent2D extent,
                                                      VkRenderPass outputRenderPass)
        : lveDevice(device)
    {
        m_currentFormat = swapChainFormat;
        m_currentExtent = extent;

        // 1. 创建离屏颜色纹理
        createOffscreenImage(swapChainFormat, extent);
        createOffscreenImageView(swapChainFormat);
        createOffscreenSampler();

        // 2. 创建离屏深度纹理
        createOffscreenDepthImage(extent);
        createOffscreenDepthImageView();

        // 3. 创建离屏 RenderPass + Framebuffer
        createOffscreenRenderPass(swapChainFormat);
        createOffscreenFramebuffer(extent);

        // 4. 创建 descriptor set layout + pool + set
        createDescriptorSetLayout();
        createDescriptorPoolAndSet();

        // 5. 创建 pipeline layout + pipeline
        createPipelineLayout();
        createPipeline(outputRenderPass);
    }

    LvePostProcessingSystem::~LvePostProcessingSystem()
    {
        cleanupResources();
    }

    void LvePostProcessingSystem::cleanupResources()
    {
        vkDeviceWaitIdle(lveDevice.device());

        if (m_pipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(lveDevice.device(), m_pipelineLayout, nullptr);

        if (m_offscreenRenderPass != VK_NULL_HANDLE)
            vkDestroyRenderPass(lveDevice.device(), m_offscreenRenderPass, nullptr);

        if (m_offscreenFramebuffer != VK_NULL_HANDLE)
            vkDestroyFramebuffer(lveDevice.device(), m_offscreenFramebuffer, nullptr);

        if (m_offscreenSampler != VK_NULL_HANDLE)
            vkDestroySampler(lveDevice.device(), m_offscreenSampler, nullptr);

        if (m_offscreenImageView != VK_NULL_HANDLE)
            vkDestroyImageView(lveDevice.device(), m_offscreenImageView, nullptr);

        if (m_offscreenImage != VK_NULL_HANDLE)
            vkDestroyImage(lveDevice.device(), m_offscreenImage, nullptr);

        if (m_offscreenMemory != VK_NULL_HANDLE)
            vkFreeMemory(lveDevice.device(), m_offscreenMemory, nullptr);

        // 清理深度资源
        if (m_offscreenDepthImageView != VK_NULL_HANDLE)
            vkDestroyImageView(lveDevice.device(), m_offscreenDepthImageView, nullptr);

        if (m_offscreenDepthImage != VK_NULL_HANDLE)
            vkDestroyImage(lveDevice.device(), m_offscreenDepthImage, nullptr);

        if (m_offscreenDepthMemory != VK_NULL_HANDLE)
            vkFreeMemory(lveDevice.device(), m_offscreenDepthMemory, nullptr);
    }

    // ============================================================
    // 重新创建（窗口大小变化时调用）
    // ============================================================
    void LvePostProcessingSystem::recreate(VkFormat swapChainFormat,
                                            VkExtent2D extent,
                                            VkRenderPass outputRenderPass)
    {
        cleanupResources();

        m_currentFormat = swapChainFormat;
        m_currentExtent = extent;
        m_offscreenImage = VK_NULL_HANDLE;
        m_offscreenMemory = VK_NULL_HANDLE;
        m_offscreenImageView = VK_NULL_HANDLE;
        m_offscreenSampler = VK_NULL_HANDLE;
        m_offscreenDepthImage = VK_NULL_HANDLE;
        m_offscreenDepthMemory = VK_NULL_HANDLE;
        m_offscreenDepthImageView = VK_NULL_HANDLE;
        m_offscreenRenderPass = VK_NULL_HANDLE;
        m_offscreenFramebuffer = VK_NULL_HANDLE;
        m_pipelineLayout = VK_NULL_HANDLE;

        createOffscreenImage(swapChainFormat, extent);
        createOffscreenImageView(swapChainFormat);
        createOffscreenSampler();
        createOffscreenDepthImage(extent);
        createOffscreenDepthImageView();
        createOffscreenRenderPass(swapChainFormat);
        createOffscreenFramebuffer(extent);
        createDescriptorPoolAndSet();
        createPipelineLayout();
        createPipeline(outputRenderPass);
    }

    // ============================================================
    // 创建离屏颜色纹理
    // ============================================================
    void LvePostProcessingSystem::createOffscreenImage(VkFormat format, VkExtent2D extent)
    {
        // 【关键修复】使用线性 HDR 格式，而非 sRGB 格式
        // 原因：
        //   1. 所有后处理（色调映射、饱和度调整等）必须在线性色彩空间中进行
        //   2. SwapChain 已是 sRGB 格式，会在写入时自动线性→sRGB 转换
        //   3. 若离屏纹理也用 sRGB，会导致多次色彩空间转换，精度严重损失
        VkFormat offscreenFormat = VK_FORMAT_R16G16B16A16_SFLOAT; // 16位浮点 HDR 格式

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = extent.width;
        imageInfo.extent.height = extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = offscreenFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                        | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        lveDevice.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                      m_offscreenImage, m_offscreenMemory);

        m_offscreenFormat = offscreenFormat;
    }

    // ============================================================
    // 创建 ImageView（颜色格式）
    // ============================================================
    void LvePostProcessingSystem::createOffscreenImageView(VkFormat format)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_offscreenImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_offscreenFormat; // 使用实际的离屏纹理格式（线性 HDR）
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(lveDevice.device(), &viewInfo, nullptr,
                              &m_offscreenImageView) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create offscreen image view!");
        }
    }

    // ============================================================
    // 创建采样器
    // ============================================================
    void LvePostProcessingSystem::createOffscreenSampler()
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.anisotropyEnable = VK_FALSE;

        if (vkCreateSampler(lveDevice.device(), &samplerInfo, nullptr,
                            &m_offscreenSampler) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create offscreen sampler!");
        }
    }

    // ============================================================
    // 创建离屏深度纹理
    // ============================================================
    void LvePostProcessingSystem::createOffscreenDepthImage(VkExtent2D extent)
    {
        // 查找支持的深度格式
        m_offscreenDepthFormat = lveDevice.findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = extent.width;
        imageInfo.extent.height = extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_offscreenDepthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        lveDevice.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                      m_offscreenDepthImage, m_offscreenDepthMemory);
    }

    // ============================================================
    // 创建深度 ImageView
    // ============================================================
    void LvePostProcessingSystem::createOffscreenDepthImageView()
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_offscreenDepthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_offscreenDepthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(lveDevice.device(), &viewInfo, nullptr,
                              &m_offscreenDepthImageView) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create offscreen depth image view!");
        }
    }

    // ============================================================
    // 创建离屏 RenderPass
    // ============================================================
    void LvePostProcessingSystem::createOffscreenRenderPass(VkFormat format)
    {
        // -------- 附件0: 颜色附件（离屏纹理）--------
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_offscreenFormat; // 使用线性 HDR 格式，与离屏纹理一致
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // -------- 附件1: 深度附件 --------
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = m_offscreenDepthFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // -------- 子通道引用 --------
        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthRef{};
        depthRef.attachment = 1;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // -------- 子通道 --------
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        // -------- 子通道依赖 --------
        std::array<VkSubpassDependency, 2> dependencies{};
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                     | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                      | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                     | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                      | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        // -------- 创建 RenderPass --------
        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        if (vkCreateRenderPass(lveDevice.device(), &renderPassInfo, nullptr,
                               &m_offscreenRenderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create offscreen render pass!");
        }
    }

    // ============================================================
    // 创建离屏 Framebuffer
    // ============================================================
    void LvePostProcessingSystem::createOffscreenFramebuffer(VkExtent2D extent)
    {
        std::array<VkImageView, 2> attachments = {
            m_offscreenImageView,       // 附件0: 颜色
            m_offscreenDepthImageView   // 附件1: 深度
        };

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_offscreenRenderPass;
        fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        fbInfo.pAttachments = attachments.data();
        fbInfo.width = extent.width;
        fbInfo.height = extent.height;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(lveDevice.device(), &fbInfo, nullptr,
                                &m_offscreenFramebuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create offscreen framebuffer!");
        }
    }

    // ============================================================
    // 创建 Descriptor Set Layout
    // ============================================================
    void LvePostProcessingSystem::createDescriptorSetLayout()
    {
        m_descriptorSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
    }

    // ============================================================
    // 创建 Descriptor Pool + 分配 Set
    // ============================================================
    void LvePostProcessingSystem::createDescriptorPoolAndSet()
    {
        m_descriptorPool = LveDescriptorPool::Builder(lveDevice)
            .setMaxSets(1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
            .build();

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_offscreenImageView;
        imageInfo.sampler = m_offscreenSampler;

        LveDescriptorWriter(*m_descriptorSetLayout, *m_descriptorPool)
            .writeImage(0, &imageInfo)
            .build(m_descriptorSet);
    }

    // ============================================================
    // 创建 Pipeline Layout
    // ============================================================
    void LvePostProcessingSystem::createPipelineLayout()
    {
        VkDescriptorSetLayout setLayout = m_descriptorSetLayout->getDescriptorSetLayout();

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &setLayout;
        layoutInfo.pushConstantRangeCount = 0;
        layoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(lveDevice.device(), &layoutInfo, nullptr,
                                   &m_pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create post processing pipeline layout!");
        }
    }

    // ============================================================
    // 创建后处理管线
    // ============================================================
    void LvePostProcessingSystem::createPipeline(VkRenderPass outputRenderPass)
    {
        PipelineConfigInfo config{};
        LvePipeline::defaultPipelineConfigInfo(config);

        config.renderPass = outputRenderPass;
        config.pipelineLayout = m_pipelineLayout;

        // 禁用深度测试
        config.depthStencilInfo.depthTestEnable = VK_FALSE;
        config.depthStencilInfo.depthWriteEnable = VK_FALSE;

        // 禁用背面剔除
        config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

        // 无顶点输入绑定
        config.bindingDescriptions.clear();
        config.attributeDescriptions.clear();

        // 启用颜色混合
        LvePipeline::enablePipelineAlphaBlending(config);

        m_pipeline = std::make_unique<LvePipeline>(
            lveDevice,
            "shaders/post_process.vert.spv",
            "shaders/post_process.frag.spv",
            config);
    }

    // ============================================================
    // 离屏 RenderPass 开始
    // ============================================================
    void LvePostProcessingSystem::beginOffscreenRenderPass(VkCommandBuffer commandBuffer) const
    {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_offscreenRenderPass;
        renderPassInfo.framebuffer = m_offscreenFramebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_currentExtent;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
        clearValues[1].depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_currentExtent.width);
        viewport.height = static_cast<float>(m_currentExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{{0, 0}, m_currentExtent};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }

    // ============================================================
    // 离屏 RenderPass 结束
    // ============================================================
    void LvePostProcessingSystem::endOffscreenRenderPass(VkCommandBuffer commandBuffer) const
    {
        vkCmdEndRenderPass(commandBuffer);
    }

    // ============================================================
    // 后处理渲染函数
    // ============================================================
    void LvePostProcessingSystem::render(FrameInfo &frameInfo)
    {
        m_pipeline->bind(frameInfo.commandBuffer);

        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipelineLayout,
            0, 1,
            &m_descriptorSet,
            0, nullptr);

        // 绘制全屏三角形（3 个顶点）
        vkCmdDraw(frameInfo.commandBuffer, 3, 1, 0, 0);
    }

} // namespace lve
