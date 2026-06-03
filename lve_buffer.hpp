// Vulkan学习项目 — Vulkan缓冲区封装
// 简化Vulkan缓冲区（VkBuffer）和关联内存的创建、映射和写入操作

#pragma once

#include "lve_device.hpp"

namespace lve
{

    // Vulkan缓冲区：管理VkBuffer和VkDeviceMemory的创建、映射和数据传输
    // 支持多实例布局，每个实例按alignmentSize对齐
    class LveBuffer
    {
    public:
        // 构造缓冲区
        // 执行步骤：
        //   1. 计算对齐后的实例大小 (instanceSize 对齐到 minOffsetAlignment)
        //   2. 分配 VkBuffer（总大小 = alignmentSize * instanceCount）
        //   3. 分配并绑定 VkDeviceMemory
        //   4. 根据 memoryPropertyFlags 决定是否持久映射 (persistent mapping)
        LveBuffer(
            LveDevice &device,
            VkDeviceSize instanceSize,
            uint32_t instanceCount,
            VkBufferUsageFlags usageFlags,
            VkMemoryPropertyFlags memoryPropertyFlags,
            VkDeviceSize minOffsetAlignment = 1);
        ~LveBuffer();

        // 禁止拷贝（Vulkan资源唯一），禁止移动（内部指针会失效）
        LveBuffer(const LveBuffer &) = delete;
        LveBuffer &operator=(const LveBuffer &) = delete;

        // 映射GPU内存到CPU可访问地址空间
        VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        // 解除内存映射
        void unmap();

        // 写入数据到整个缓冲区
        void writeToBuffer(void *data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        // 刷新非一致内存（non-coherent memory），使CPU写入对GPU可见
        VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        // 获取描述符缓冲区信息（用于VkWriteDescriptorSet）
        VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        // 使GPU写入对CPU可见
        VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

        // 按实例索引写入数据（offset = index * alignmentSize）
        void writeToIndex(void *data, int index);
        VkResult flushIndex(int index);
        VkDescriptorBufferInfo descriptorInfoForIndex(int index);
        VkResult invalidateIndex(int index);

        // 获取底层Vulkan资源句柄和配置
        VkBuffer getBuffer() const { return buffer; }
        void *getMappedMemory() const { return mapped; }
        uint32_t getInstanceCount() const { return instanceCount; }
        VkDeviceSize getInstanceSize() const { return instanceSize; }
        VkDeviceSize getAlignmentSize() const { return instanceSize; }
        VkBufferUsageFlags getUsageFlags() const { return usageFlags; }
        VkMemoryPropertyFlags getMemoryPropertyFlags() const { return memoryPropertyFlags; }
        VkDeviceSize getBufferSize() const { return bufferSize; }

        // 将缓冲区内容复制到Vulkan图像（使用暂存缓冲区方式）
        void copyBufferToImage(VkImage image, uint32_t width, uint32_t height);

    private:
        // 计算对齐后的实例大小（向上取整到minOffsetAlignment的倍数）
        static VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

        LveDevice &lveDevice;
        void *mapped = nullptr;                    // CPU可访问的映射内存指针
        VkBuffer buffer = VK_NULL_HANDLE;          // Vulkan缓冲区句柄
        VkDeviceMemory memory = VK_NULL_HANDLE;    // 关联的设备内存

        VkDeviceSize bufferSize;                   // 总缓冲区大小
        uint32_t instanceCount;                    // 实例数量
        VkDeviceSize instanceSize;                 // 单个实例大小（对齐前）
        VkDeviceSize alignmentSize;                // 单个实例对齐后的大小
        VkBufferUsageFlags usageFlags;             // 缓冲区用途标志
        VkMemoryPropertyFlags memoryPropertyFlags; // 内存属性标志
    };

} // namespace lve
