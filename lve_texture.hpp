// Vulkan学习项目 — 纹理类
// 管理Vulkan图像、图像视图和采样器的创建，支持2D纹理和立方体贴图

#pragma once
#include "lve_device.hpp"
#include <string>
#include <memory>
namespace lve
{
    // Vulkan纹理：封装VkImage、VkImageView和VkSampler的完整纹理管线
    class LveTexture
    {
    public:
        // 默认构造函数（创建一个空的纹理对象，后续可通过loadFromFile填充）
        LveTexture(LveDevice &device);

        // 从文件加载纹理（支持jpg/png等常见格式，通过stb_image解码）
        LveTexture(LveDevice &device, const std::string &filepath);

        ~LveTexture();

        // 禁止拷贝（Vulkan资源唯一），允许移动
        LveTexture(const LveTexture &) = delete;
        LveTexture &operator=(const LveTexture &) = delete;
        LveTexture(LveTexture &&other) noexcept;
        LveTexture &operator=(LveTexture &&other) noexcept;

        // 从文件加载纹理数据
        // 执行步骤：
        //   1. 通过stb_image加载图像文件解码为RGBA像素数据
        //   2. 创建GPU本地暂存缓冲区（staging buffer）
        //   3. 创建目标VkImage（传输目标 + 着色器采样）
        //   4. 转换图像布局并拷贝数据
        //   5. 创建图像视图和采样器
        void loadFromFile(const std::string &filepath);

        // 创建空纹理（指定尺寸和格式，未填充像素数据，用于离屏渲染等）
        void createEmpty(uint32_t width, uint32_t height, VkFormat format);

        // 从6个面文件创建立方体贴图（用于天空盒）
        static std::shared_ptr<LveTexture> createCubemap(
            LveDevice &device,
            const std::array<std::string, 6> &filepaths);

        // 从等距矩形投影图（equirectangular）转换为立方体贴图
        static std::shared_ptr<LveTexture> createCubemapFromEquirectangular(
            LveDevice &device,
            const std::string &equirectangularPath);

        // 判断纹理是否完整有效（image、imageView、sampler均有效）
        bool isValid() const
        {
            return image != VK_NULL_HANDLE &&
                   imageView != VK_NULL_HANDLE &&
                   sampler != VK_NULL_HANDLE;
        }
        bool isCubemap() const { return m_isCubemap; }

        VkImageView getImageView() const { return imageView; }
        VkSampler getSampler() const { return sampler; }
        uint32_t getWidth() const { return width; }
        uint32_t getHeight() const { return height; }

    private:
        // 创建VkImage和分配设备内存
        void createImage(uint32_t width, uint32_t height, VkFormat format,
                         VkImageTiling tiling, VkImageUsageFlags usage,
                         VkMemoryPropertyFlags properties);
        // 创建VkImageView（指定格式和aspect）
        void createImageView(VkFormat format, VkImageAspectFlags aspectFlags);
        // 创建VkSampler（默认使用线性过滤和重复寻址模式）
        void createImageSampler();
        // 转换图像布局（通过管线屏障）
        void transitionImageLayout(VkImage image, VkFormat format,
                                   VkImageLayout oldLayout, VkImageLayout newLayout);
        void cleanup(); // 清理所有Vulkan资源

        LveDevice &lveDevice;
        VkImage image = VK_NULL_HANDLE;           // Vulkan图像句柄
        VkDeviceMemory imageMemory = VK_NULL_HANDLE; // 关联的设备内存
        VkImageView imageView = VK_NULL_HANDLE;   // 图像视图（定义格式和访问范围）
        VkSampler sampler = VK_NULL_HANDLE;       // 采样器（定义过滤和寻址模式）
        uint32_t width = 0;                       // 纹理宽度
        uint32_t height = 0;                      // 纹理高度
        bool m_isCubemap = false;                 // 是否为立方体贴图
    };
}
