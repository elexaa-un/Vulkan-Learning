#include "lve_shadow_system.hpp"
#include "../lve_buffer.hpp"
#include <stdexcept>
#include <iostream>
#include <array>

namespace lve
{
    // ============================================================
    // 构造函数
    // ============================================================
    // 执行步骤：
    //   1. 创建 Shadow Map 图像 (2048x2048 深度图)
    //   2. 创建 ImageView（着色器采样用）
    //   3. 创建 Sampler（带比较功能，实现硬件 PCF）
    //   4. 创建独立的 Render Pass（只写深度，不写颜色）
    //   5. 创建 Framebuffer（绑定深度图像）
    //   6. 创建 Pipeline Layout + Pipeline
    // ============================================================
    LveShadowSystem::LveShadowSystem(LveDevice &device, VkDescriptorSetLayout globalSetLayout)
        : lveDevice(device)
    {
        createShadowMapImage();
        createShadowMapImageView();
        createShadowMapSampler();
        createShadowRenderPass();
        createShadowFramebuffer();
        createPipelineLayout(globalSetLayout);
        createPipeline();
    }

    // ============================================================
    // 析构函数
    // ============================================================
    LveShadowSystem::~LveShadowSystem()
    {
        vkDeviceWaitIdle(lveDevice.device());

        if (m_pipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(lveDevice.device(), m_pipelineLayout, nullptr);

        if (m_shadowRenderPass != VK_NULL_HANDLE)
            vkDestroyRenderPass(lveDevice.device(), m_shadowRenderPass, nullptr);

        if (m_shadowFramebuffer != VK_NULL_HANDLE)
            vkDestroyFramebuffer(lveDevice.device(), m_shadowFramebuffer, nullptr);

        if (m_shadowMapSampler != VK_NULL_HANDLE)
            vkDestroySampler(lveDevice.device(), m_shadowMapSampler, nullptr);

        if (m_shadowMapImageView != VK_NULL_HANDLE)
            vkDestroyImageView(lveDevice.device(), m_shadowMapImageView, nullptr);

        if (m_shadowMapImage != VK_NULL_HANDLE)
            vkDestroyImage(lveDevice.device(), m_shadowMapImage, nullptr);

        if (m_shadowMapMemory != VK_NULL_HANDLE)
            vkFreeMemory(lveDevice.device(), m_shadowMapMemory, nullptr);
    }

    // ============================================================
    // 创建 Shadow Map 图像
    // ============================================================
    // Shadow Map 是一张 2048x2048 的深度图
    // 格式：D32_SFLOAT（32位浮点深度）
    // 用途：深度附件（写入）+ 采样（着色器读取）
    // ============================================================
    void LveShadowSystem::createShadowMapImage()
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = SHADOW_MAP_WIDTH;
        imageInfo.extent.height = SHADOW_MAP_HEIGHT;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_D32_SFLOAT;           // 32位浮点深度
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;        // GPU 最优布局
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT  // 可作为深度附件
                        | VK_IMAGE_USAGE_SAMPLED_BIT;                 // 可在着色器中采样
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        lveDevice.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                      m_shadowMapImage, m_shadowMapMemory);
    }

    // ============================================================
    // 创建 ImageView
    // ============================================================
    // 告诉 Vulkan 如何解读这张图像：
    //   - 2D 图像（非数组、非立方体）
    //   - 深度格式
    //   - 只有深度方面（没有模板）
    // ============================================================
    void LveShadowSystem::createShadowMapImageView()
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_shadowMapImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_D32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(lveDevice.device(), &viewInfo, nullptr,
                              &m_shadowMapImageView) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shadow map image view!");
        }
    }

    // ============================================================
    // 创建采样器（带比较功能 → 硬件 PCF）
    // ============================================================
    // 关键设计：
    //   compareEnable = VK_TRUE        ← 启用深度比较
    //   compareOp = LESS_OR_EQUAL      ← 深度值 ≤ 记录值 → 可见(1.0)
    //
    // 硬件 PCF 原理：
    //   - 当着色器调用 texture(samplerShadow, ...) 时
    //   - GPU 自动对比采样值和你传入的参考值
    //   - 返回 0.0（完全阴影）到 1.0（完全照亮）的浮点值
    //   - 线性过滤 + 比较 = 免费的 2x2 PCF 软阴影
    // ============================================================
    void LveShadowSystem::createShadowMapSampler()
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;          // 放大时线性过滤
        samplerInfo.minFilter = VK_FILTER_LINEAR;          // 缩小时线性过滤
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;  // 边缘钳制
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        // ============ 深度比较（Shadow Map 的核心）============
        samplerInfo.compareEnable = VK_TRUE;
        samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        // ========================================================

        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;  // 超出范围的返回白色（无阴影）
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.anisotropyEnable = VK_FALSE;       // 深度图不需要各向异性过滤
        samplerInfo.maxAnisotropy = 1.0f;

        if (vkCreateSampler(lveDevice.device(), &samplerInfo, nullptr,
                            &m_shadowMapSampler) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shadow map sampler!");
        }
    }

    // ============================================================
    // 创建 Shadow Render Pass
    // ============================================================
    // 这个 Render Pass 和主渲染 Pass 不同：
    //   1. 没有颜色附件（只写深度）
    //   2. 深度附件 = Shadow Map 图像
    //   3. 最终布局 = SHADER_READ_ONLY_OPTIMAL（渲染后采样用）
    // ============================================================
    void LveShadowSystem::createShadowRenderPass()
    {
        // -------- 深度附件描述 --------
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = VK_FORMAT_D32_SFLOAT;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;        // 开始时清除为 1.0（最远）
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;      // 保留深度数据
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // 最终布局 → 着色器只读，这样主渲染通道可以采样
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        // -------- 子通道引用 --------
        VkAttachmentReference depthRef{};
        depthRef.attachment = 0;  // 附件索引 0
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;  // 渲染时的布局

        // -------- 子通道 --------
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 0;            // 没有颜色附件！
        subpass.pColorAttachments = nullptr;
        subpass.pDepthStencilAttachment = &depthRef;  // 只有深度附件

        // -------- 子通道依赖（同步）--------
        // 两个依赖很重要：
        //   依赖1: 外部 → 子通道（等待深度附件可用）
        //   依赖2: 子通道 → 外部（等待深度写入完成，才能被着色器采样）
        std::array<VkSubpassDependency, 2> dependencies{};

        // 依赖 1：确保在深度附件可用后才开始渲染
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        // 依赖 2：确保深度写入完成后，着色器才能采样
        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        // -------- 创建 RenderPass --------
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &depthAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        if (vkCreateRenderPass(lveDevice.device(), &renderPassInfo, nullptr,
                               &m_shadowRenderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shadow render pass!");
        }
    }

    // ============================================================
    // 创建 Framebuffer
    // ============================================================
    // Framebuffer 把 Render Pass 的附件和实际的 ImageView 绑定在一起。
    // 这里只有一个附件 → Shadow Map 的 ImageView。
    // ============================================================
    void LveShadowSystem::createShadowFramebuffer()
    {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_shadowRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &m_shadowMapImageView;
        fbInfo.width = SHADOW_MAP_WIDTH;
        fbInfo.height = SHADOW_MAP_HEIGHT;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(lveDevice.device(), &fbInfo, nullptr,
                                &m_shadowFramebuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shadow framebuffer!");
        }
    }

    // ============================================================
    // 创建 Pipeline Layout
    // ============================================================
    // Push Constant 布局（64 字节）：
    //   - modelMatrix (mat4 = 64 bytes) → 顶点着色器
    //   - lightViewProj (mat4 = 64 bytes) → 顶点着色器
    //
    // 注意：单个 push constant 不能超过 maxPushConstantSize（通常是 128 字节）
    //       这里总共 128 字节，刚好在限制内。
    // ============================================================
    void LveShadowSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
    {
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.offset = 0;
        // modelMatrix(64) + lightViewProj(64) = 128 bytes
        pushRange.size = sizeof(glm::mat4) * 2;

        // Shadow Pass 不需要描述符集（没有 UBO、没有纹理）
        // 只需要 push constant 来传递矩阵
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 0;            // 无描述符集
        layoutInfo.pSetLayouts = nullptr;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;

        if (vkCreatePipelineLayout(lveDevice.device(), &layoutInfo, nullptr,
                                   &m_pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shadow pipeline layout!");
        }
    }

    // ============================================================
    // 创建 Pipeline
    // ============================================================
    // 关键配置：
    //   - 顶点着色器：shadow.vert（变换到光源裁剪空间）
    //   - 片段着色器：无！（不需要输出颜色，只写深度）
    //   - 颜色混合：禁用（没有颜色附件）
    //   - 剔除模式：FRONT（Peter Panning 问题 → 渲染背面来生成阴影）
    //   - 深度偏移：启用（Shadow Acne 问题 → 添加微小偏移）
    // ============================================================
    void LveShadowSystem::createPipeline()
    {
        PipelineConfigInfo config{};
        LvePipeline::defaultPipelineConfigInfo(config);

        config.renderPass = m_shadowRenderPass;
        config.pipelineLayout = m_pipelineLayout;

        // ========== 颜色混合：禁用 ==========
        // Shadow Pass 没有颜色附件，所以颜色混合无关紧要
        // 但仍然需要显式设置（defaultPipelineConfigInfo 设置了默认值）
        config.colorBlendAttachment.colorWriteMask = 0;   // 不写颜色
        // =====================================

        // ========== 剔除模式：剔除正面 ==========
        // 为什么剔除正面（FRONT）而不是背面（BACK）？
        // → 这是解决 "Peter Panning"（阴影脱离物体）问题的常用技巧。
        //   渲染背面意味着阴影体积略微"膨胀"，消除物体和阴影间的缝隙。
        config.rasterizationInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
        // =======================================

        // ========== 深度偏移（在光栅化状态上设置！）==========
        // 解决 "Shadow Acne"（阴影条纹）问题。
        // 原理：光栅化时给每个片段加一个微小偏移，
        //       防止物体自身的面与 Shadow Map 中的值冲突。
        config.rasterizationInfo.depthBiasEnable = VK_TRUE;
        config.rasterizationInfo.depthBiasConstantFactor = 1.25f;  // 常数偏移
        config.rasterizationInfo.depthBiasSlopeFactor = 1.75f;     // 斜率相关偏移
        // ============================

        // 使用 shadow.vert 和 shadow.frag 创建管线
        m_pipeline = std::make_unique<LvePipeline>(
            lveDevice,
            "shaders/shadow.vert.spv",
            "shaders/shadow.frag.spv",  // 片段着色器（空实现，深度由硬件写入）
            config);
    }

    // ============================================================
    // 主渲染函数：每帧调用一次
    // ============================================================
    // 流程：
    //   1. 计算光源的 View 和 Projection 矩阵
    //   2. 开始 Shadow Render Pass（清除深度为 1.0）
    //   3. 遍历所有物体，从光源视角渲染（只写深度）
    //   4. 结束 Shadow Render Pass
    //
    // 参数：
    //   lightPos    — 光源位置（通常是场景中的点光源位置）
    //   lightTarget — 光源照射目标（通常是场景中心）
    // ============================================================
    void LveShadowSystem::render(FrameInfo &frameInfo,
                                  glm::vec3 lightPos, glm::vec3 lightTarget)
    {
        // ===== 第 1 步：计算光源的 View 投影矩阵 =====
        // lookAt: 从光源位置看向场景中心
        glm::mat4 lightView = glm::lookAt(lightPos, lightTarget, glm::vec3(0.0f, 1.0f, 0.0f));

        // 正交投影：适合方向光（太阳光）。
        // 透视投影也可以，但正交投影阴影更均匀。
        float orthoSize = 25.0f;  // 覆盖范围（世界单位）
        glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize,
                                         -orthoSize, orthoSize,
                                         0.1f, 100.0f);  // 近平面 0.1，远平面 100

        // Vulkan 使用深度范围 [0, 1]，需要修正投影矩阵的 Y 轴
        lightProj[1][1] *= -1;

        m_lightViewProj = lightProj * lightView;

        // ===== 第 2 步：开始 Shadow Render Pass =====
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_shadowRenderPass;
        renderPassInfo.framebuffer = m_shadowFramebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = {SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT};

        // 清除深度为 1.0（最远距离，表示没有物体遮挡）
        VkClearValue clearValue{};
        clearValue.depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(frameInfo.commandBuffer, &renderPassInfo,
                             VK_SUBPASS_CONTENTS_INLINE);

        // 设置视口（和 Shadow Map 尺寸一致）
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(SHADOW_MAP_WIDTH);
        viewport.height = static_cast<float>(SHADOW_MAP_HEIGHT);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(frameInfo.commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT};
        vkCmdSetScissor(frameInfo.commandBuffer, 0, 1, &scissor);

        // ===== 第 3 步：从光源视角渲染所有物体 =====
        m_pipeline->bind(frameInfo.commandBuffer);

        for (auto &kv : frameInfo.gameObjects)
        {
            auto &obj = kv.second;
            if (obj.model == nullptr) continue;

            // ShadowPush 结构体（与 shaders/shadow.vert 对齐）
            struct ShadowPush
            {
                glm::mat4 modelMatrix;
                glm::mat4 lightViewProj;
            } push;

            push.modelMatrix = obj.transform.mat4();
            push.lightViewProj = m_lightViewProj;

            vkCmdPushConstants(frameInfo.commandBuffer,
                               m_pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(ShadowPush),
                               &push);

            obj.model->bind(frameInfo.commandBuffer);
            obj.model->draw(frameInfo.commandBuffer);
        }

        // ===== 第 4 步：结束 Shadow Render Pass =====
        vkCmdEndRenderPass(frameInfo.commandBuffer);

        // Shadow Map 现在处于 DEPTH_STENCIL_READ_ONLY_OPTIMAL 布局
        // 主渲染通道的着色器可以安全采样了
    }

} // namespace lve
