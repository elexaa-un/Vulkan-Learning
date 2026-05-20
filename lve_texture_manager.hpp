#pragma once
#include "lve_device.hpp"
#include <string>
#include <memory>

namespace lve
{
    class LveTextureManager
    {
    public:
        /// @brief Initialize fallback resources: creates a 1x1 white texture
        ///        image view and a default sampler. Must be called once after
        ///        the Vulkan device is created.
        static void initFallbackResources(VkDevice device, VkPhysicalDevice physicalDevice)
        {
            s_fallbackSampler = createDefaultSampler(device, physicalDevice);
            s_fallbackImageView = createWhiteImageView(device, physicalDevice);
        }

        /// @brief Destroy all fallback resources. Call during cleanup.
        static void cleanupFallbackResources(VkDevice device);

        static VkSampler getFallbackSampler() { return s_fallbackSampler; }
        static VkImageView getFallbackImageView() { return s_fallbackImageView; }

    private:
        /// @brief Create a minimal default sampler (linear filtering, repeat).
        static VkSampler createDefaultSampler(VkDevice device, VkPhysicalDevice physicalDevice);

        /// @brief Create a 1x1 white RGBA8 image and return its image view.
        static VkImageView createWhiteImageView(VkDevice device, VkPhysicalDevice physicalDevice);

        static VkSampler s_fallbackSampler;
        static VkImageView s_fallbackImageView;
        static VkImage s_fallbackImage;
        static VkDeviceMemory s_fallbackImageMemory;
    };
}
