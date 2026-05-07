#pragma once
#include "lve_device.hpp"
#include <string>
#include <memory>
namespace lve
{
    class LveTexture
    {
    public:
        // 默认构造函数（创建一个空的纹理对象）
        LveTexture(LveDevice &device);

        // 从文件加载纹理
        LveTexture(LveDevice &device, const std::string &filepath);

        ~LveTexture();

        // 禁止拷贝
        LveTexture(const LveTexture &) = delete;
        LveTexture &operator=(const LveTexture &) = delete;

        // 允许移动
        LveTexture(LveTexture &&other) noexcept;
        LveTexture &operator=(LveTexture &&other) noexcept;

        // 从文件加载（可用于重新初始化）
        void loadFromFile(const std::string &filepath);

        // 创建空纹理（指定尺寸和格式，未填充数据）
        void createEmpty(uint32_t width, uint32_t height, VkFormat format);

        // 创建立方体贴图
        static std::shared_ptr<LveTexture> createCubemap(
            LveDevice &device,
            const std::array<std::string, 6> &filepaths);

        static std::shared_ptr<LveTexture> createCubemapFromEquirectangular(
            LveDevice &device,
            const std::string &equirectangularPath);

        // 判断是否为有效纹理
        bool isValid() const { return image != VK_NULL_HANDLE; }
        bool isCubemap() const { return m_isCubemap; }

        VkImageView getImageView() const { return imageView; }
        VkSampler getSampler() const { return sampler; }
        uint32_t getWidth() const { return width; }
        uint32_t getHeight() const { return height; }

    private:
        void createImage(uint32_t width, uint32_t height, VkFormat format,
                         VkImageTiling tiling, VkImageUsageFlags usage,
                         VkMemoryPropertyFlags properties);
        void createImageView(VkFormat format, VkImageAspectFlags aspectFlags);
        void createImageSampler();
        void transitionImageLayout(VkImage image, VkFormat format,
                                   VkImageLayout oldLayout, VkImageLayout newLayout);
        void cleanup(); // 清理资源的辅助函数

        LveDevice &lveDevice;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory imageMemory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        uint32_t width = 0;
        uint32_t height = 0;
        bool m_isCubemap = false;
    };
}