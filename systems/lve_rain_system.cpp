// Vulkan学习项目 — 雨滴粒子系统实现（精简版）

#include "lve_rain_system.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <ctime>
#include <random>
#include <fstream>

namespace lve
{
    static std::vector<char> readFile(const std::string &path)
    {
        std::ifstream f(path, std::ios::ate | std::ios::binary);
        if (!f.is_open()) throw std::runtime_error("failed to open: " + path);
        size_t sz = static_cast<size_t>(f.tellg());
        std::vector<char> buf(sz);
        f.seekg(0); f.read(buf.data(), sz); f.close();
        return buf;
    }

    LveRainSystem::LveRainSystem(LveDevice &device, VkRenderPass renderPass,
                                 VkDescriptorSetLayout globalSetLayout,
                                 uint32_t maxParticles, LveModel *quadModel)
        : lveDevice(device), m_maxParticles(maxParticles), m_quadModel(quadModel)
    {
        std::cout << "[RainSystem] Creating " << maxParticles << " rain particles..." << std::endl;

        createBuffers();
        loadTexture();
        createDescriptorSets();
        createComputePipeline();
        createGraphicsPipeline(renderPass, globalSetLayout);

        // 初始化粒子数据
        std::default_random_engine rng(static_cast<unsigned>(std::time(nullptr)));
        std::uniform_real_distribution<float> d01(0.0f, 1.0f);

        LveBuffer staging(lveDevice, sizeof(RainParticle), m_maxParticles,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        staging.map();
        for (uint32_t i = 0; i < m_maxParticles; ++i) {
            RainParticle p{};
            p.position = glm::vec4(-50.0f + d01(rng) * 100.0f, 10.0f + d01(rng) * 30.0f,
                                   -50.0f + d01(rng) * 100.0f, 0.0f);
            p.velocity = glm::vec4(-0.15f + d01(rng) * 0.3f, -5.0f - d01(rng) * 10.0f,
                                   -0.15f + d01(rng) * 0.3f, d01(rng) * 8.0f);
            float size = 0.05f + d01(rng) * 0.10f;
            p.startColor = glm::vec4(1.0f, 1.0f, 1.0f, size);
            p.endColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.1f);
            p.endColor.w = 2.0f + d01(rng) * 6.0f;
            p.color = p.startColor;
            staging.writeToIndex(&p, i);
        }
        staging.unmap();
        lveDevice.copyBuffer(staging.getBuffer(), m_particleBuffer->getBuffer(),
                             sizeof(RainParticle) * m_maxParticles);

        std::cout << "[RainSystem] Initialized " << m_maxParticles << " rain particles." << std::endl;
    }

    LveRainSystem::~LveRainSystem()
    {
        if (m_computePipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(lveDevice.device(), m_computePipeline, nullptr);
        if (m_computePipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(lveDevice.device(), m_computePipelineLayout, nullptr);
        if (m_graphicsPipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(lveDevice.device(), m_graphicsPipelineLayout, nullptr);
    }

    void LveRainSystem::createBuffers()
    {
        m_particleBuffer = std::make_unique<LveBuffer>(
            lveDevice, sizeof(RainParticle), m_maxParticles,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_uboBuffer = std::make_unique<LveBuffer>(
            lveDevice, sizeof(RainUbo), 1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        m_uboBuffer->map();
    }

    void LveRainSystem::loadTexture()
    {
        m_texture = std::make_unique<LveTexture>(lveDevice, "rain.png");
    }

    void LveRainSystem::createDescriptorSets()
    {
        m_computeSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
            .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT).build();
        m_computePool = LveDescriptorPool::Builder(lveDevice)
            .setMaxSets(1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1).build();
        auto ssbo = m_particleBuffer->descriptorInfo();
        auto ubo = m_uboBuffer->descriptorInfo();
        LveDescriptorWriter(*m_computeSetLayout, *m_computePool)
            .writeBuffer(0, &ssbo).writeBuffer(1, &ubo).build(m_computeSet);

        m_textureSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).build();
        m_texturePool = LveDescriptorPool::Builder(lveDevice)
            .setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1).build();
        VkDescriptorImageInfo img{};
        img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img.imageView = m_texture->getImageView();
        img.sampler = m_texture->getSampler();
        LveDescriptorWriter(*m_textureSetLayout, *m_texturePool).writeImage(0, &img).build(m_textureSet);
    }

    void LveRainSystem::createComputePipeline()
    {
        VkDescriptorSetLayout lay = m_computeSetLayout->getDescriptorSetLayout();
        VkPipelineLayoutCreateInfo li{};
        li.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        li.setLayoutCount = 1; li.pSetLayouts = &lay;
        if (vkCreatePipelineLayout(lveDevice.device(), &li, nullptr, &m_computePipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("rain compute layout failed");

        auto code = readFile("shaders/particle_rain.comp.spv");
        VkShaderModuleCreateInfo sm{};
        sm.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        sm.codeSize = code.size(); sm.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule mod;
        if (vkCreateShaderModule(lveDevice.device(), &sm, nullptr, &mod) != VK_SUCCESS)
            throw std::runtime_error("rain comp shader module failed");

        VkPipelineShaderStageCreateInfo ss{};
        ss.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ss.stage = VK_SHADER_STAGE_COMPUTE_BIT; ss.module = mod; ss.pName = "main";

        VkComputePipelineCreateInfo pi{};
        pi.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pi.stage = ss; pi.layout = m_computePipelineLayout;
        if (vkCreateComputePipelines(lveDevice.device(), VK_NULL_HANDLE, 1, &pi, nullptr, &m_computePipeline) != VK_SUCCESS)
            throw std::runtime_error("rain compute pipeline failed");
        vkDestroyShaderModule(lveDevice.device(), mod, nullptr);
    }

    void LveRainSystem::createGraphicsPipeline(VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
    {
        std::array<VkDescriptorSetLayout, 2> layouts = {globalSetLayout, m_textureSetLayout->getDescriptorSetLayout()};
        VkPipelineLayoutCreateInfo li{};
        li.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        li.setLayoutCount = static_cast<uint32_t>(layouts.size()); li.pSetLayouts = layouts.data();
        if (vkCreatePipelineLayout(lveDevice.device(), &li, nullptr, &m_graphicsPipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("rain graphics layout failed");

        PipelineConfigInfo cfg{};
        LvePipeline::defaultPipelineConfigInfo(cfg);
        cfg.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        cfg.depthStencilInfo.depthWriteEnable = VK_FALSE;
        cfg.depthStencilInfo.depthTestEnable = VK_TRUE;
        LvePipeline::enablePipelineAlphaBlending(cfg);

        std::vector<VkVertexInputBindingDescription> bindings(2);
        bindings[0] = LveModel::Vertex::getBindingDescriptions()[0];
        bindings[1].binding = 1; bindings[1].stride = sizeof(RainParticle);
        bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        auto attrs = LveModel::Vertex::getAttributeDescriptions();

        VkVertexInputAttributeDescription pa{};
        pa.location = 4; pa.binding = 1; pa.format = VK_FORMAT_R32G32B32_SFLOAT;
        pa.offset = offsetof(RainParticle, position);
        VkVertexInputAttributeDescription ca{};
        ca.location = 5; ca.binding = 1; ca.format = VK_FORMAT_R32G32B32_SFLOAT;
        ca.offset = offsetof(RainParticle, color);
        VkVertexInputAttributeDescription sa{};
        sa.location = 6; sa.binding = 1; sa.format = VK_FORMAT_R32_SFLOAT;
        sa.offset = offsetof(RainParticle, color) + sizeof(float) * 3;
        attrs.push_back(pa); attrs.push_back(ca); attrs.push_back(sa);

        cfg.bindingDescriptions = bindings; cfg.attributeDescriptions = attrs;
        cfg.vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
        cfg.vertexInputInfo.pVertexBindingDescriptions = bindings.data();
        cfg.vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
        cfg.vertexInputInfo.pVertexAttributeDescriptions = attrs.data();
        cfg.pipelineLayout = m_graphicsPipelineLayout; cfg.renderPass = renderPass;

        m_graphicsPipeline = std::make_unique<LvePipeline>(
            lveDevice, "shaders/particle_rain.vert.spv", "shaders/particle_rain.frag.spv", cfg);
    }

    void LveRainSystem::updateAndRender(FrameInfo &frameInfo,
                                         const glm::vec3 &emitterMin, const glm::vec3 &emitterMax,
                                         const WeatherPreset &weather, uint32_t activeCount)
    {
        if (activeCount == 0 || weather.isSnow) return;

        RainUbo ubo{};
        ubo.emitterPositionMin = emitterMin; ubo.emitterPositionMax = emitterMax;
        ubo.deltaTime = frameInfo.frameTime;
        ubo.gravity = -15.0f * weather.precipitationRate;
        ubo.windDirection = glm::vec3(weather.cloudWindVelocity.x, 0.0f, weather.cloudWindVelocity.y);
        ubo.windStrength = 0.3f * weather.precipitationRate;
        ubo.cameraPosition = frameInfo.camera.getPosition();
        ubo.particleCount = activeCount; ubo.time = static_cast<float>(glfwGetTime());
        ubo.dropletSize = weather.dropletSize; ubo.dropletSpeed = weather.dropletSpeed;
        m_uboBuffer->writeToBuffer(&ubo);

        VkCommandBuffer cmd = frameInfo.commandBuffer;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout,
                                0, 1, &m_computeSet, 0, nullptr);
        vkCmdDispatch(cmd, (activeCount + 255) / 256, 1, 1);

        VkBufferMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.buffer = m_particleBuffer->getBuffer(); b.offset = 0; b.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0,
                             0, nullptr, 1, &b, 0, nullptr);

        m_graphicsPipeline->bind(cmd);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout,
                                0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout,
                                1, 1, &m_textureSet, 0, nullptr);
        m_quadModel->bind(cmd);
        VkBuffer ib[] = {m_particleBuffer->getBuffer()};
        VkDeviceSize off[] = {0};
        vkCmdBindVertexBuffers(cmd, 1, 1, ib, off);
        vkCmdDrawIndexed(cmd, m_quadModel->getIndexCount(), activeCount, 0, 0, 0);
    }

} // namespace lve