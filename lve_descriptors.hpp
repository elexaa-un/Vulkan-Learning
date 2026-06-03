// Vulkan学习项目 — 描述符系统
// 封装Vulkan描述符集布局、描述符池和描述符写入操作

#pragma once

#include "lve_device.hpp"

// std
#include <memory>
#include <unordered_map>
#include <vector>

namespace lve
{

    // 描述符集布局：定义着色器资源绑定的结构（类似函数签名）
    class LveDescriptorSetLayout
    {
    public:
        // Builder模式：逐步添加绑定后构建布局
        class Builder
        {
        public:
            Builder(LveDevice &lveDevice) : lveDevice{lveDevice} {}

            // 添加一个描述符绑定
            // 参数：binding — 绑定点编号, descriptorType — 描述符类型, stageFlags — 可见着色器阶段, count — 数组元素数量
            Builder &addBinding(
                uint32_t binding,
                VkDescriptorType descriptorType,
                VkShaderStageFlags stageFlags,
                uint32_t count = 1);

            // 构建并返回描述符集布局对象
            std::unique_ptr<LveDescriptorSetLayout> build() const;

            const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> &getBindings() const
            {
                return bindings;
            }

        private:
            LveDevice &lveDevice;
            std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{};
        };

        // 构造函数：根据绑定表创建VkDescriptorSetLayout
        LveDescriptorSetLayout(
            LveDevice &lveDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
        ~LveDescriptorSetLayout();
        LveDescriptorSetLayout(const LveDescriptorSetLayout &) = delete;
        LveDescriptorSetLayout &operator=(const LveDescriptorSetLayout &) = delete;

        VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

    private:
        LveDevice &lveDevice;
        VkDescriptorSetLayout descriptorSetLayout; // Vulkan描述符集布局句柄
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings; // 绑定资源信息

        friend class LveDescriptorWriter; // 允许Writer访问内部布局
    };

    // 描述符池：管理描述符集的内存分配
    class LveDescriptorPool
    {
    public:
        // Builder模式：配置池大小后构建
        class Builder
        {
        public:
            Builder(LveDevice &lveDevice) : lveDevice{lveDevice} {}

            // 添加某种描述符类型的池容量
            Builder &addPoolSize(VkDescriptorType descriptorType, uint32_t count);
            // 设置池标志（如 VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT）
            Builder &setPoolFlags(VkDescriptorPoolCreateFlags flags);
            // 设置最大描述符集数量
            Builder &setMaxSets(uint32_t count);
            std::unique_ptr<LveDescriptorPool> build() const;

        private:
            LveDevice &lveDevice;
            std::vector<VkDescriptorPoolSize> poolSizes{};
            uint32_t maxSets = 1000;                // 默认最大描述符集数量
            VkDescriptorPoolCreateFlags poolFlags = 0;
        };

        LveDescriptorPool(
            LveDevice &lveDevice,
            uint32_t maxSets,
            VkDescriptorPoolCreateFlags poolFlags,
            const std::vector<VkDescriptorPoolSize> &poolSizes);
        ~LveDescriptorPool();
        LveDescriptorPool(const LveDescriptorPool &) = delete;
        LveDescriptorPool &operator=(const LveDescriptorPool &) = delete;
        VkDescriptorPool getDescriptorPool() const { return descriptorPool; }

        // 从池中分配一个描述符集
        bool allocateDescriptor(
            const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const;

        // 释放描述符集回池
        void freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const;

        // 重置整个描述符池（释放所有已分配的描述符集）
        void resetPool();

    private:
        LveDevice &lveDevice;
        VkDescriptorPool descriptorPool; // Vulkan描述符池句柄

        friend class LveDescriptorWriter;
    };

    // 描述符写入器：用于方便地向描述符集中写入Ubo和纹理绑定
    class LveDescriptorWriter
    {
    public:
        LveDescriptorWriter(LveDescriptorSetLayout &setLayout, LveDescriptorPool &pool);

        // 写入缓冲区描述符（UBO/SSBO）
        LveDescriptorWriter &writeBuffer(uint32_t binding, VkDescriptorBufferInfo *bufferInfo);
        // 写入图像描述符（纹理采样器）
        LveDescriptorWriter &writeImage(uint32_t binding, VkDescriptorImageInfo *imageInfo);

        // 构建描述符集（首次分配并写入）
        bool build(VkDescriptorSet &set);
        // 覆盖已有描述符集（更新已分配的描述符集内容）
        void overwrite(VkDescriptorSet &set);

    private:
        LveDescriptorSetLayout &setLayout;
        LveDescriptorPool &pool;
        std::vector<VkWriteDescriptorSet> writes; // 待执行的写入操作列表

        // 保存 image/buffer info 结构体的副本
        // 确保写入描述符引用的指针在调用 vkUpdateDescriptorSets 前保持有效
        std::vector<VkDescriptorImageInfo> imageInfos;
        std::vector<VkDescriptorBufferInfo> bufferInfos;
    };

} // namespace lve
