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
    // Shadow Map 系统
    // ============================================================
    // 作用：从光源视角渲染场景到一张深度纹理 (Shadow Map) 中。
    //       在主渲染通道中，着色器采样这张纹理来判断像素是否在阴影中。
    //
    // 核心流程：
    //   1. beginShadowPass() → 切换到 Shadow Render Pass
    //   2. renderGameObjects() → 从光源视角绘制所有物体，只写深度
    //   3. endShadowPass() → 结束 Shadow Pass
    //   4. getShadowMapImageView() / getShadowMapSampler() → 主渲染时采样
    // ============================================================
    class LveShadowSystem
    {
    public:
        // 阴影贴图分辨率（越大阴影质量越高，但性能消耗也越大）
        static constexpr uint32_t SHADOW_MAP_WIDTH = 2048;
        static constexpr uint32_t SHADOW_MAP_HEIGHT = 2048;

        LveShadowSystem(LveDevice &device, VkDescriptorSetLayout globalSetLayout);
        ~LveShadowSystem();

        LveShadowSystem(const LveShadowSystem &) = delete;
        LveShadowSystem &operator=(const LveShadowSystem &) = delete;

        // ========== 公共接口 ==========

        // 获取阴影贴图的 ImageView（写入到 global descriptor set 的 binding 2）
        VkImageView getShadowMapImageView() const { return m_shadowMapImageView; }
        VkSampler getShadowMapSampler() const { return m_shadowMapSampler; }

        // 获取光源的 View*Projection 矩阵（着色器需要用它来计算阴影坐标）
        glm::mat4 getLightViewProj() const { return m_lightViewProj; }

        // 渲染阴影贴图（一帧调用一次）
        // frameInfo: 包含 commandBuffer、gameObjects 等
        // lightPos: 光源位置（世界空间）
        // lightTarget: 光源照射目标（世界空间）
        void render(FrameInfo &frameInfo, glm::vec3 lightPos, glm::vec3 lightTarget);

    private:
        // ========== 初始化步骤 ==========
        void createShadowMapImage();       // 创建深度图像 + 内存
        void createShadowMapImageView();   // 创建 ImageView
        void createShadowMapSampler();     // 创建采样器（用于主渲染时采样）
        void createShadowRenderPass();     // 创建 Shadow Pass 的 RenderPass
        void createShadowFramebuffer();    // 创建 Framebuffer（绑定深度图像）
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline();

        // 图像布局转换（深度附件写入 ? 着色器只读采样）
        void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);

        LveDevice &lveDevice;

        // ========== Shadow Map 资源 ==========
        VkImage m_shadowMapImage = VK_NULL_HANDLE;
        VkDeviceMemory m_shadowMapMemory = VK_NULL_HANDLE;
        VkImageView m_shadowMapImageView = VK_NULL_HANDLE;
        VkSampler m_shadowMapSampler = VK_NULL_HANDLE;

        // ========== Render Pass 资源 ==========
        VkRenderPass m_shadowRenderPass = VK_NULL_HANDLE;
        VkFramebuffer m_shadowFramebuffer = VK_NULL_HANDLE;

        // ========== Pipeline 资源 ==========
        std::unique_ptr<LvePipeline> m_pipeline;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

        // ========== 光源变换矩阵 ==========
        glm::mat4 m_lightViewProj{1.0f};
    };
}
