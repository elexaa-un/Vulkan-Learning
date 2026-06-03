// Vulkan学习项目 — 图形管线
// 管理Vulkan图形管线（VkPipeline）和着色器模块的创建与绑定

#pragma once
#include <string>
#include <vector>
#include "lve_device.hpp"
namespace lve
{
    // 管线配置信息：包含图形管线创建所需的所有状态配置
    struct PipelineConfigInfo
    {
        PipelineConfigInfo() = default;
        // 禁止拷贝（配置信息可能很大），仅允许移动
        PipelineConfigInfo(const PipelineConfigInfo &) = delete;
        PipelineConfigInfo &operator=(const PipelineConfigInfo &) = delete;

        std::vector<VkVertexInputBindingDescription> bindingDescriptions{};    // 顶点输入绑定描述
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};// 顶点输入属性描述
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};               // 顶点输入状态
        VkPipelineViewportStateCreateInfo viewportInfo{};                      // 视口状态
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};         // 输入装配状态（拓扑类型）
        VkPipelineRasterizationStateCreateInfo rasterizationInfo = {};         // 光栅化状态
        VkPipelineMultisampleStateCreateInfo multisampleInfo = {};             // 多重采样状态
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};         // 颜色混合附件状态
        VkPipelineColorBlendStateCreateInfo colorBlendInfo = {};               // 颜色混合状态
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {};           // 深度/模板测试状态
        std::vector<VkDynamicState> dynamicStateEnables{};                     // 启用的动态状态列表
        VkPipelineDynamicStateCreateInfo dynamicStateInfo{};                   // 动态状态
        VkPipelineLayout pipelineLayout = nullptr;                             // 管线布局（描述符集布局）
        VkRenderPass renderPass = nullptr;                                     // 渲染通道
        uint32_t subpass = 0;                                                  // 子通道索引
    };

    // 图形管线：封装 VkPipeline 和着色器模块的创建流程
    class LvePipeline
    {
    public:
        // 构造函数：从SPIR-V着色器文件创建图形管线
        // 执行步骤：
        //   1. 创建顶点着色器模块（readFile + vkCreateShaderModule）
        //   2. 创建片元着色器模块
        //   3. 配置着色器阶段信息
        //   4. 调用 vkCreateGraphicsPipelines 创建管线
        LvePipeline(LveDevice &device,
                    const std::string vertFilepath,
                    const std::string fragFilepath,
                    const PipelineConfigInfo &configInfo);
        ~LvePipeline();

        LvePipeline(const LvePipeline &) = delete;
        LvePipeline &operator=(const LvePipeline &) = delete;

        // 绑定管线到命令缓冲区（vkCmdBindPipeline）
        void bind(VkCommandBuffer commandBuffer);

        // 填充默认管线配置（三角形列表、视口/裁剪、背面剔除等）
        static void defaultPipelineConfigInfo(PipelineConfigInfo &configInfo);

        // 启用Alpha混合（用于透明渲染）
        static void enablePipelineAlphaBlending(PipelineConfigInfo &configInfo);

    private:
        // 读取二进制SPIR-V着色器文件
        static std::vector<char> readFile(const std::string filepath);

        // 创建图形管线的完整流程
        void createGraphicsPipeline(const std::string &vertFilePath,
                                    const std::string &fragFilePath,
                                    const PipelineConfigInfo &configInfo);

        // 创建VkShaderModule（从SPIR-V字节码）
        void createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule);

        LveDevice &lveDevice;
        VkPipeline graphicsPipeline;         // 图形管线句柄
        VkShaderModule vertShaderModule;     // 顶点着色器模块
        VkShaderModule fragShaderModule;     // 片元着色器模块
    };
}
