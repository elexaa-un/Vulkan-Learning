#include "lve_pipeline.hpp"
#include "lve_model.hpp"
#include "lve_utils.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>
namespace lve
{
    LvePipeline::LvePipeline(LveDevice &device,
                             const std::string vertFilepath,
                             const std::string fragFilepath,
                             const PipelineConfigInfo &configInfo) : lveDevice{device}
    {
        createGraphicsPipeline(vertFilepath, fragFilepath, configInfo);
    }

    LvePipeline::~LvePipeline()
    {
        vkDestroyShaderModule(lveDevice.device(), vertShaderModule, nullptr);
        vkDestroyShaderModule(lveDevice.device(), fragShaderModule, nullptr);
        vkDestroyPipeline(lveDevice.device(), graphicsPipeline, nullptr);
    }
    std::vector<char> LvePipeline::readFile(const std::string filepath)
    {
        std::ifstream file{filepath, std::ios::ate | std::ios::binary};
        if (!file.is_open())
        {
            throw std::runtime_error("failed to open file:" + filepath);
        }
        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();
        return buffer;
    }
    void LvePipeline::bind(VkCommandBuffer commandBuffer)
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    }
    void LvePipeline::createGraphicsPipeline(const std::string &vertFilePath, const std::string &fragFilePath, const PipelineConfigInfo &configInfo)
    {
        LVE_ASSERT(
            configInfo.pipelineLayout != VK_NULL_HANDLE,
            "Cannot create graphics pipeline::no pipelineLayout provided in configInfo");
        LVE_ASSERT(
            configInfo.renderPass != VK_NULL_HANDLE,
            "Cannot create graphics pipeline::no renderpass provided in configInfo");
        auto vertCode = readFile(vertFilePath);
        auto fragCode = readFile(fragFilePath);

        createShaderModule(vertCode, &vertShaderModule);
        createShaderModule(fragCode, &fragShaderModule);

        VkPipelineShaderStageCreateInfo shaderStages[2];
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vertShaderModule;
        shaderStages[0].pName = "main";
        shaderStages[0].flags = 0;
        shaderStages[0].pNext = nullptr;
        shaderStages[0].pSpecializationInfo = nullptr;

        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = fragShaderModule;
        shaderStages[1].pName = "main";
        shaderStages[1].flags = 0;
        shaderStages[1].pNext = nullptr;
        shaderStages[1].pSpecializationInfo = nullptr;

        auto bindingDescriptions = configInfo.bindingDescriptions;
        auto attributeDescriptions = configInfo.attributeDescriptions;

        VkPipelineVertexInputStateCreateInfo vertexInfo{};
        vertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        vertexInfo.pVertexBindingDescriptions = bindingDescriptions.data();

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInfo;
        pipelineInfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;
        pipelineInfo.pViewportState = &configInfo.viewportInfo;
        pipelineInfo.pRasterizationState = &configInfo.rasterizationInfo;
        pipelineInfo.pColorBlendState = &configInfo.colorBlendInfo;
        pipelineInfo.pDepthStencilState = &configInfo.depthStencilInfo;
        pipelineInfo.pMultisampleState = &configInfo.multisampleInfo;
        pipelineInfo.pDynamicState = &configInfo.dynamicStateInfo;

        pipelineInfo.layout = configInfo.pipelineLayout;
        pipelineInfo.renderPass = configInfo.renderPass;
        pipelineInfo.subpass = configInfo.subpass;

        // 这里无用处，这两个参数可用于性能优化
        pipelineInfo.basePipelineIndex = -1;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        VK_CHECK(vkCreateGraphicsPipelines(lveDevice.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline));
    }

    void LvePipeline::createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule)
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

        VK_CHECK(vkCreateShaderModule(lveDevice.device(), &createInfo, nullptr, shaderModule));
    }

    void LvePipeline::defaultPipelineConfigInfo(PipelineConfigInfo &configInfo)
    {
        configInfo.inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
        configInfo.inputAssemblyInfo.pNext = nullptr;
        configInfo.inputAssemblyInfo.flags = 0;

        configInfo.viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        configInfo.viewportInfo.viewportCount = 1;
        configInfo.viewportInfo.pViewports = nullptr;
        configInfo.viewportInfo.scissorCount = 1;
        configInfo.viewportInfo.pScissors = nullptr;

        // 光栅化
        configInfo.rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        configInfo.rasterizationInfo.depthClampEnable = VK_FALSE;
        configInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
        configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
        configInfo.rasterizationInfo.lineWidth = 1.0f;
        configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        configInfo.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        configInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;
        configInfo.rasterizationInfo.depthBiasConstantFactor = 0.0f; // Optional
        configInfo.rasterizationInfo.depthBiasClamp = 0.0f;          // Optional
        configInfo.rasterizationInfo.depthBiasSlopeFactor = 0.0f;    // Optional

        // 多重采样，避免锯齿
        configInfo.multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        configInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;
        configInfo.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        configInfo.multisampleInfo.minSampleShading = 1.0f;          // Optional
        configInfo.multisampleInfo.pSampleMask = nullptr;            // Optional
        configInfo.multisampleInfo.alphaToCoverageEnable = VK_FALSE; // Optional
        configInfo.multisampleInfo.alphaToOneEnable = VK_FALSE;      // Optional
        configInfo.multisampleInfo.flags = 0;

        // 颜色混合
        configInfo.colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        configInfo.colorBlendAttachment.blendEnable = VK_FALSE;
        configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
        configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;             // Optional
        configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
        configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;             // Optional

        configInfo.colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        configInfo.colorBlendInfo.logicOpEnable = VK_FALSE;
        configInfo.colorBlendInfo.logicOp = VK_LOGIC_OP_COPY; // Optional
        configInfo.colorBlendInfo.attachmentCount = 1;
        configInfo.colorBlendInfo.pAttachments = &configInfo.colorBlendAttachment;
        configInfo.colorBlendInfo.blendConstants[0] = 0.0f; // Optional
        configInfo.colorBlendInfo.blendConstants[1] = 0.0f; // Optional
        configInfo.colorBlendInfo.blendConstants[2] = 0.0f; // Optional
        configInfo.colorBlendInfo.blendConstants[3] = 0.0f; // Optional

        // 深度测试
        configInfo.depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;
        configInfo.depthStencilInfo.depthWriteEnable = VK_TRUE;
        configInfo.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        configInfo.depthStencilInfo.minDepthBounds = 0.0f; // Optional
        configInfo.depthStencilInfo.maxDepthBounds = 1.0f; // Optional
        configInfo.depthStencilInfo.stencilTestEnable = VK_TRUE;
        configInfo.depthStencilInfo.front = {}; // Optional
        configInfo.depthStencilInfo.back = {};  // Optional
        configInfo.depthStencilInfo.pNext = nullptr;
        configInfo.depthStencilInfo.flags = 0;

        configInfo.dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        configInfo.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        configInfo.dynamicStateInfo.pDynamicStates = configInfo.dynamicStateEnables.data();
        configInfo.dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(configInfo.dynamicStateEnables.size());
        configInfo.dynamicStateInfo.flags = 0;

        configInfo.bindingDescriptions = LveModel::Vertex::getBindingDescriptions();
        configInfo.attributeDescriptions = LveModel::Vertex::getAttributeDescriptions();
    }

    void LvePipeline::enablePipelineAlphaBlending(PipelineConfigInfo &configInfo)
    {
        configInfo.colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        configInfo.colorBlendAttachment.blendEnable = VK_TRUE;
        configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;           // Optional
        configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA; // Optional
        configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;                            // Optional
        configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;                 // Optional
        configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;                // Optional
        configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;                            // Optional
    }
}
